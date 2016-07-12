#include "bosswave.h"

#include "allocations.h"

#include <QFile>
#include <QQmlEngine>

#include <msgpack.h>

#include <cstdio>

class NotImplementedException: public std::exception
{
public:
    NotImplementedException(const char* msg)
    {
        this->msg = msg;
    }

    virtual const char* what() const throw()
    {
        return this->msg;
    }

private:
    const char* msg;
};

class BadRouterMessageException: public std::exception
{
public:
    BadRouterMessageException(const char* msg, const char* detail)
    {
        size_t totallen = (size_t) strlen(msg) + (size_t) strlen(detail) + Q_UINT64_C(3);
        char* scratch = new char[totallen];
        snprintf(scratch, totallen, "%s: %s", msg, detail);
        this->msg = scratch;
    }

    ~BadRouterMessageException()
    {
        delete[] this->msg;
    }

    virtual const char* what() const throw()
    {
        return this->msg;
    }

private:
    const char* msg;
};

Res<QString> BW::_nop_res_status;

BW::BW(QObject *parent):
    QObject(parent)
{
    //_nop_res_status = std::function<void(QString)>([](QString){});
    _nop_res_status = [](QString){};
    m_agent = NULL;
}

BW::~BW()
{
}

QObject *BW::qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    auto rv = BW::instance();
    rv->engine = engine;
    rv->jsengine = scriptEngine;
    return rv;
}
BW *BW::instance()
{
    static BW *bw = new BW();
    return bw;
}

void BW::setEntityFile(QString filename, Res<QString> on_done)
{
    QFile f(filename);
    qDebug() << "the filename is" << f.fileName();
    if (!f.open(QIODevice::ReadOnly))
    {
        on_done(QString("Could not open entity file: %1").arg(f.errorString()));
        return;
    }
    QByteArray contents = f.readAll().mid(1);
    setEntity(contents, on_done);
}

int BW::fromDF(QString df)
{
    QStringList l = df.split('.');
    Q_ASSERT(l.length() == 4);
    int rv = 0;
    rv += l[0].toInt() << 24;
    rv += l[1].toInt() << 16;
    rv += l[2].toInt() << 8;
    rv += l[3].toInt();
    return rv;
}

void BW::setEntityFromEnviron(Res<QString> on_done)
{
    QByteArray a = qgetenv("BW2_DEFAULT_ENTITY");
    if (a.isEmpty()) {
        on_done("BW2_DEFAULT_ENTITY not set");
        return;
    }
    setEntityFile(a.data(), on_done);
}

void BW::connectAgent(QString host, quint16 port)
{
    if (m_agent != NULL)
    {
        m_agent->deleteLater();
        m_agent = NULL;
    }
    m_agent = new AgentConnection(host, port);
    connect(m_agent,&AgentConnection::agentChanged,this,&BW::agentChanged);
    m_agent->beginConnection();
}

AgentConnection* BW::agent()
{
    if (m_agent == NULL)
    {
        qFatal("Use of BW without connection to agent.");
    }
    return m_agent;
}

void BW::setEntity(QByteArray &contents, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::SET_ENTITY);
    auto po = createBasePayloadObject(bwpo::num::ROEntityWKey, contents);
    f->addPayloadObject(po);
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if(f->checkResponse(on_done))
        {
            m_vk = f->getHeaderS("vk");
            qDebug() << "VK is" << m_vk;
            on_done("");
        }
    });
}

void BW::createEntity(QDateTime expiry, qreal expiryDelta, QString contact,
                      QString comment, QList<QString> revokers, bool omitCreationDate,
                      Res<QString, QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::MAKE_ENTITY);
    if (expiry.isValid())
    {
        /* The format is supposed to follow RFC 3339. The format I'm
         * using here looks exactly the same, even though it is named
         * "ISODate".
         */
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta", QStringLiteral("%1ms").arg(expiryDelta));
    }
    f->addHeader("contact", contact);
    f->addHeader("comment", comment);
    for (auto i = revokers.begin(); i != revokers.end(); i++)
    {
        f->addHeader("revoker", *i);
    }
    if (omitCreationDate)
    {
        f->addHeader("omitcreationdate", "true");
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), QStringLiteral("")))
        {
            PayloadObject* po = f->getPayloadObjects().value(0);
            if (po == nullptr)
            {
                on_done("invalid reponse", "", "");
                return;
            }
            QString vk = f->getHeaderS("vk");
            on_done("", vk, po->contentArray());
        }
    });
}

void BW::createDOT(bool isPermission, QString to, unsigned int ttl, QDateTime expiry,
                   qreal expiryDelta, QString contact, QString comment,
                   QList<QString> revokers, bool omitCreationDate, QString uri,
                   QString accessPermissions, QVariantMap appPermissions,
                   Res<QString, QString, QString> on_done)
{
    // appPermissions is for Permission DOTs, which are not yet supported
    Q_UNUSED(appPermissions);

    auto f = agent()->newFrame(Frame::MAKE_DOT);
    if (expiry.isValid())
    {
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta",QStringLiteral("%1ms").arg(expiryDelta));
    }
    f->addHeader("contact", contact);
    f->addHeader("comment", comment);
    for (auto i = revokers.begin(); i != revokers.end(); i++)
    {
        f->addHeader("revoker", *i);
    }
    if (omitCreationDate)
    {
        f->addHeader("omitcreationdate", "true");
    }
    f->addHeader("ttl", QString::number(ttl));
    f->addHeader("to", to);
    if (isPermission)
    {
        f->addHeader("ispermission", QStringLiteral("true"));
        throw NotImplementedException("Permission DOTs are not yet supported");
    }
    else
    {
        f->addHeader("ispermission", QStringLiteral("false"));
        f->addHeader("uri", uri);
        f->addHeader("accesspermissions", accessPermissions);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), QStringLiteral("")))
        {
            PayloadObject* po = f->getPayloadObjects().value(0);
            if (po == nullptr)
            {
                on_done("invalid response", "", "");
            }
            QString hash = f->getHeaderS("hash");
            on_done("", hash, po->contentArray());
        }
    });
}

void BW::buildChain(QString uri, QString permissions, QString to,
                    Res<QString, SimpleChain*, bool> on_done)
{
    auto f = agent()->newFrame(Frame::BUILD_CHAIN);
    f->addHeader("uri", uri);
    f->addHeader("to", to);
    f->addHeader("addpermissions", permissions);
    agent()->transact(this, f, [=](PFrame f, bool final)
    {
        SimpleChain* sc = new SimpleChain;
        sc->valid = false;
        if (f->checkResponse(on_done, sc, true))
        {
            QVariantMap chain;
            QString hash = f->getHeaderS("hash");
            if (hash != QStringLiteral(""))
            {
                sc->valid = true;
                sc->hash = hash;
                sc->permissions = f->getHeaderS("permissions");
                sc->to = f->getHeaderS("to");
                sc->uri = f->getHeaderS("uri");
                PayloadObject* po = f->getPayloadObjects().value(0);
                if (po == nullptr)
                {
                    sc->content = QString("");
                }
                else
                {
                    sc->content = QString(po->contentArray());
                }
            }
            on_done("", sc, final);
        }
    });
}

void BW::buildAnyChain(QString uri, QString permissions, QString to,
                       Res<QString, SimpleChain*> on_done)
{
    bool* calledCB = new bool;
    *calledCB = false;

    this->buildChain(uri, permissions, to, [=](QString error, SimpleChain* chain, bool final)
    {
        if (!*calledCB)
        {
            on_done(error, chain);
            *calledCB = true;
        }

        if (final)
        {
            delete calledCB;
        }
    });
}

void BW::publish(QString uri, QString primaryAccessChain, bool autoChain, QList<PayloadObject*> poz,
                 QDateTime expiry, qreal expiryDelta, QString elaboratePAC, bool doNotVerify,
                 bool persist, Res<QString> on_done)
{
    const char* cmd = persist ? Frame::PERSIST : Frame::PUBLISH;
    auto f = agent()->newFrame(cmd);
    if (autoChain)
    {
        f->addHeader("autochain", "true");
    }
    if (expiry.isValid())
    {
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta", QStringLiteral("%1ms").arg(expiryDelta));
    }
    f->addHeader("uri", uri);
    if (primaryAccessChain.length() != 0)
    {
        f->addHeader("primary_access_chain", primaryAccessChain);
    }

    foreach(auto po, poz)
    {
        f->addPayloadObject(po);
    }

    if (elaboratePAC.length() == 0)
    {
        elaboratePAC = elaboratePartial;
    }

    f->addHeader("elaborate_pac", elaboratePAC);
    f->addHeader("doverify", doNotVerify ? "false" : "true");
    f->addHeader("persist", persist ? "true" : "false");

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if(f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                        QString PODF, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        Res<QString> on_done)
{
    publishMsgPack(uri, primaryAccessChain, autoChain, BW::fromDF(PODF), val, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, on_done);
}

void BW::publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                        int ponum, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        Res<QString> on_done)
{
    QByteArray contents = MsgPack::pack(val);
    PayloadObject* po = createBasePayloadObject(ponum, contents.data(), contents.length());
    publish(uri, primaryAccessChain, autoChain, {po}, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, on_done);
}

void BW::publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                        int PONum, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        QJSValue on_done)
{
    publishMsgPack(uri, primaryAccessChain, autoChain, PONum, val, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, Res<QString>(on_done));
}

void BW::publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                        QString PODF, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        QJSValue on_done)
{
    publishMsgPack(uri, primaryAccessChain, autoChain, BW::fromDF(PODF), val, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, Res<QString>(on_done));
}

void BW::publishText(QString uri, QString msg, Res<QString> on_done)
{
    publishText(uri, "", true, bwpo::num::Text, msg, QDateTime(), -1, "", false, false, on_done);
}
void BW::publishText(QString uri, QString msg, QJSValue on_done)
{
    publishText(uri, msg, Res<QString>(on_done));
}

void BW::publishText(QString uri, QString primaryAccessChain, bool autoChain,
                     int PONum, QString msg, QDateTime expiry, qreal expiryDelta,
                     QString elaboratePAC, bool doNotVerify, bool persist,
                     Res<QString> on_done)
{
    QByteArray encoded = msg.toUtf8();
    PayloadObject* po = createBasePayloadObject(PONum, encoded);
    publish(uri, primaryAccessChain, autoChain, {po}, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, on_done);
}

void BW::publishText(QString uri, QString primaryAccessChain, bool autoChain,
                     int poNum, QString msg, QDateTime expiry, qreal expiryDelta,
                     QString elaboratePAC, bool doNotVerify, bool persist,
                     QJSValue on_done)
{
    publishText(uri, primaryAccessChain, autoChain, poNum, msg, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, Res<QString>(on_done));
}


void BW::query(QString uri, QString primaryAccessChain, bool autoChain,
               QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
               bool doNotVerify, bool leavePacked,
               Res<QString, QList<PMessage>> on_done)
{
    auto f = agent()->newFrame(Frame::QUERY);
    if (autoChain)
    {
        f->addHeader("autochain", "true");
    }
    if (expiry.isValid())
    {
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta", QString::number(expiryDelta));
    }
    f->addHeader("uri",uri);
    if (primaryAccessChain.length() != 0)
    {
        f->addHeader("primary_access_chain", primaryAccessChain);
    }
    if (elaboratePAC.length() == 0)
    {
        elaboratePAC = elaboratePartial;
    }
    f->addHeader("elaborate_pac", elaboratePAC);
    if (!leavePacked)
    {
        f->addHeader("unpack", "true");
    }
    f->addHeader("doverify", doNotVerify ? "false": "true");

    auto lst = new QList<PMessage>();
    agent()->transact(this, f, [=](PFrame f, bool final)
    {
        if (f->isType(Frame::RESPONSE))
        {
            f->checkResponse(on_done, QList<PMessage>());
        }
        else
        {
            if (final)
            {
                on_done("", *lst);
                delete lst;
            }
            else
            {
                lst->append(Message::fromFrame(f));
            }
        }
    });
}

void BW::subscribe(QString uri, QString primaryAccessChain, bool autoChain,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked, Res<PMessage> on_msg,
                   Res<QString> on_done, Res<QString> on_handle)
{
    auto f = agent()->newFrame(Frame::SUBSCRIBE);
    if (autoChain)
    {
        f->addHeader("autochain", "true");
    }
    if (expiry.isValid())
    {
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta", QString::number(expiryDelta));
    }
    f->addHeader("uri",uri);
    if (primaryAccessChain.length() != 0)
    {
        f->addHeader("primary_access_chain", primaryAccessChain);
    }
    if (elaboratePAC.length() == 0)
    {
        elaboratePAC = elaboratePartial;
    }
    f->addHeader("elaborate_pac", elaboratePAC);
    if (!leavePacked)
    {
        f->addHeader("unpack", "true");
    }
    f->addHeader("doverify", doNotVerify ? "false": "true");
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->isType(Frame::RESPONSE))
        {
            QString handle = f->getHeaderS("handle");
            on_handle(handle);

            if(f->checkResponse(on_done))
            {
                qDebug() << "invoking nil reply";
                on_done("");
            }
            else
            {
                qDebug() << "not invoking nil reply";
            }
        }
        else
        {
            on_msg(Message::fromFrame(f));
        }
    });
}

void BW::subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, bool leavePacked, Res<QVariantMap> on_msg, Res<QString> on_done, Res<QString> on_handle)
{
    BW::subscribe(uri, primaryAccessChain, autoChain, expiry,
                  expiryDelta, elaboratePAC, doNotVerify, leavePacked,
                  [=](PMessage m)
    {
        foreach(auto po, m->FilterPOs(bwpo::num::MsgPack, bwpo::mask::MsgPack))
        {
            QVariant v = MsgPack::unpack(po->contentArray());
            on_msg(v.toMap());
        }
    }, on_done, on_handle);
}

void BW::subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, bool leavePacked, QJSValue on_msg)
{
    subscribeMsgPack(uri, primaryAccessChain, autoChain, expiry,
                     expiryDelta, elaboratePAC, doNotVerify, leavePacked,
                     ERes<QVariantMap>(on_msg));
}

void BW::subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, bool leavePacked, QJSValue on_msg, QJSValue on_done)
{
    subscribeMsgPack(uri, primaryAccessChain, autoChain, expiry,
                     expiryDelta, elaboratePAC, doNotVerify, leavePacked,
                     ERes<QVariantMap>(on_msg), ERes<QString>(on_done));
}

void BW::unsubscribe(QString handle, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::UNSUBSCRIBE);
    f->addHeader("handle", handle);
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            qDebug() << "invoking nil reply";
            on_done("");
        }
    });
}

void BW::unsubscribe(QString handle, QJSValue on_done)
{
    unsubscribe(handle, ERes<QString>(on_done));
}

void BW::list(QString uri, QString primaryAccessChain, bool autoChain,
              QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
              bool doNotVerify, Res<QString, QString, bool> on_done)
{
    auto f = agent()->newFrame(Frame::MAKE_ENTITY);
    if (autoChain)
    {
        f->addHeader("autochain", "true");
    }
    if (expiry.isValid())
    {
        /* The format is supposed to follow RFC 3339. The format I'm
         * using here looks exactly the same, even though it is named
         * "ISODate".
         */
        f->addHeader("expiry", expiry.toString(Qt::ISODate));
    }
    if (expiryDelta >= 0)
    {
        f->addHeader("expirydelta", QStringLiteral("%1ms").arg(expiryDelta));
    }
    f->addHeader("uri", uri);
    if (primaryAccessChain.length() != 0)
    {
        f->addHeader("primary_access_chain", primaryAccessChain);
    }
    if (elaboratePAC == QStringLiteral(""))
    {
        elaboratePAC = elaboratePartial;
    }
    f->addHeader("elaborate_pac", elaboratePAC);
    f->addHeader("doverify", doNotVerify ? QStringLiteral("false") : QStringLiteral("true"));

    agent()->transact(this, f, [=](PFrame f, bool final)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), true))
        {
            bool ok;
            QString child = f->getHeaderS("child", &ok);
            if (ok || final)
            {
                on_done("", child, final);
            }
        }
    });
}

void BW::publishDOTWithAcc(QByteArray blob, int account,
                           Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::PUT_DOT);
    PayloadObject* po = PayloadObject::load(bwpo::num::ROAccessDOT, blob.constData(), blob.size());
    f->addPayloadObject(po);
    f->addHeader("account", QString::number(account));

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral("")))
        {
            QString hash = f->getHeaderS("hash");
            on_done("", hash);
        }
    });
}

void BW::publishDOT(QByteArray blob, Res<QString, QString> on_done)
{
    this->publishDOTWithAcc(blob, 0, on_done);
}

void BW::publishEntityWithAcc(QByteArray blob, int account,
                              Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::PUT_ENTITY);
    PayloadObject* po = PayloadObject::load(bwpo::num::ROEntity, blob.constData(), blob.size());
    f->addPayloadObject(po);
    f->addHeader("account", QString::number(account));

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral("")))
        {
            QString hash = f->getHeaderS("vk");
            on_done("", hash);
        }
    });
}

void BW::publishEntity(QByteArray blob, Res<QString, QString> on_done)
{
    this->publishEntityWithAcc(blob, 0, on_done);
}

void BW::setMetadata(QString uri, QString key, QString val, Res<QString> on_done)
{
    QVariantMap metadata;
    metadata["val"] = val;
    metadata["ts"] = QDateTime::currentMSecsSinceEpoch() * Q_INT64_C(1000000);

    if (uri.endsWith(QStringLiteral("/")))
    {
        uri.chop(1);
    }
    uri += "/!meta/";
    uri += key;

    this->publishMsgPack(uri, "", true, bwpo::num::SMetadata, metadata, QDateTime(), -1, "", false, true, on_done);
}

void BW::delMetadata(QString uri, QString key, Res<QString> on_done)
{
    if (uri.endsWith(QStringLiteral("/")))
    {
        uri.chop(1);
    }
    uri += "/!meta/";
    uri += key;

    this->publish(uri, "", true, QList<PayloadObject*>(), QDateTime(), -1, "", false, true, on_done);
}

struct metadata_kv
{
    QString k;
    QVariantMap m;
    QString o;
};

struct metadata_info
{
    QVector<struct metadata_kv> chans;
    int numreturned;
    int errorhappened;
};

void BW::getMetadata(QString uri, Res<QString, QVariantMap, QVariantMap> on_tuple)
{
    QStringList parts = uri.split('/', QString::SkipEmptyParts);
    QString turi("");

    struct metadata_info* mi = new struct metadata_info;
    mi->numreturned = 0;
    mi->errorhappened = false;

    int li = 0;
    for (auto i = parts.begin(); i != parts.end(); i++)
    {
        turi += *i;
        turi += "/";

        QString touse(turi);
        touse += "!meta/";

        this->query(touse, "", true, QDateTime(), -1, "",
                    false, false, [=](QString error, QList<PMessage> messages)
        {

            if (error.length() != 0)
            {
                on_tuple(error, QVariantMap(), QVariantMap());
                mi->errorhappened = true;
            }
            else
            {
                struct metadata_kv& metadata = mi->chans[li];

                metadata.o = turi;
                for (auto j = messages.begin(); j != messages.end(); j++)
                {
                    PMessage& sm = *j;
                    QString uri = sm->getHeaderS("uri");
                    QStringList uriparts = uri.split('/');
                    metadata.k = uriparts.last();
                    QList<PayloadObject*> pos = sm->FilterPOs(bwpo::num::SMetadata, bwpo::mask::SMetadata);
                    for (auto k = pos.begin(); k != pos.end(); k++)
                    {
                        QVariant dictv = MsgPack::unpack((*k)->contentArray());
                        QVariantMap dict = dictv.toMap();
                        metadata.m = dict;
                    }
                }
            }

            if (++mi->numreturned == mi->chans.length())
            {
                if (!mi->errorhappened)
                {
                    /* Call on_tuple. */
                    QVariantMap rvO;
                    QVariantMap rvM;

                    for (auto res = mi->chans.begin(); res != mi->chans.end(); res++)
                    {
                        rvO[res->k] = res->o;
                        rvM[res->k] = res->m;
                    }

                    on_tuple("", rvM, rvO);
                }

                delete mi;
            }
        });

        li++;
    }
}

void BW::publishChainWithAcc(QByteArray blob, int account, Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::PUT_CHAIN);
    PayloadObject* po = PayloadObject::load(bwpo::num::ROAccessDChain, blob.constData(), blob.size());
    f->addPayloadObject(po);
    f->addHeader("account", QString::number(account));

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral("")))
        {
            QString hash = f->getHeaderS("vk");
            on_done("", hash);
        }
    });
}

void BW::publishChain(QByteArray blob, Res<QString, QString> on_done)
{
    this->publishChainWithAcc(blob, 0, on_done);
}

void BW::unresolveAlias(QByteArray blob, Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::RESOLVE_ALIAS);
    f->addHeaderB("unresolve", blob);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral("")))
        {
            QString v = f->getHeaderS("value");
            on_done("", v);
        }
    });
}

void BW::resolveLongAlias(QString al, Res<QString, QByteArray, bool> on_done)
{
    auto f = agent()->newFrame(Frame::RESOLVE_ALIAS);
    f->addHeader("longkey", al);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QByteArray(), false))
        {
            QByteArray v = f->getHeaderB("value");
            on_done("", v, v.count('\0') == v.size());
        }
    });
}

void BW::resolveShortAlias(QString al, Res<QString, QByteArray, bool> on_done)
{
    auto f = agent()->newFrame(Frame::RESOLVE_ALIAS);
    f->addHeader("shortkey", al);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QByteArray(), false))
        {
            QByteArray v = f->getHeaderB("value");
            on_done("", v, v.count('\0') == v.size());
        }
    });
}

void BW::resolveEmbeddedAlias(QString al, Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::RESOLVE_ALIAS);
    f->addHeader("longkey", al);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral("")))
        {
            QString v = f->getHeaderS("value");
            on_done("", v);
        }
    });
}

void BW::resolveRegistry(QString key, Res<QString, RoutingObject*, RegistryValidity> on_done)
{
    auto f = agent()->newFrame(Frame::RESOLVE_REGISTRY);
    f->addHeader("key", key);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, (RoutingObject*) nullptr, RegistryValidity::StateError))
        {
            QList<RoutingObject*> ros = f->getRoutingObjects();
            if (ros.length() == 0)
            {
                on_done("", nullptr, RegistryValidity::StateError);
                return;
            }
            QString valid = f->getHeaderS("validity");
            RegistryValidity validity;

            if (valid == "valid")
            {
                validity = RegistryValidity::StateValid;
            }
            else if (valid == "expired")
            {
                validity = RegistryValidity::StateExpired;
            }
            else if (valid == "revoked")
            {
                validity = RegistryValidity::StateRevoked;
            }
            else if (valid == "unknown")
            {
                validity = RegistryValidity::StateUnknown;
            }
            else
            {
                std::string rawvalid = valid.toStdString();
                throw BadRouterMessageException("Invalid \"validity\" value", rawvalid.data());
            }

            on_done("", ros.first(), validity);
        }
    });
}

QString BW::getVK()
{
    return m_vk;
}

void BW::createView(QVariantMap query, Res<QString, BWView*> on_done)
{
    auto f = agent()->newFrame(Frame::MAKE_VIEW);
    QByteArray mpo = MsgPack::pack(query);
    f->addHeaderB("msgpack",mpo);
    BWView* rv = new BWView(this);
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->isType(Frame::RESPONSE))
        {
            if(f->checkResponse(on_done, (BWView*)nullptr))
            {
                qDebug() << "invoking nil reply";
                rv->m_vid = f->getHeaderI("id");
                on_done("", rv);
                rv->onChange();
            }
        }
        else
        {
            rv->onChange();
        }
    });
}

void BW::createView(QVariantMap query, QJSValue on_done)
{
    createView(query, [=](QString s, BWView *v) mutable
    {
        QJSValueList jsl;
        jsl.append(QJSValue(s));
        jsl.append(engine->toScriptValue((QObject*)v));
        on_done.call(jsl);
    });
}

void BWView::onChange()
{
    auto f = bw->agent()->newFrame(Frame::LIST_VIEW);
    f->addHeader("id",QString::number(m_vid));
    bw->agent()->transact(this, f, [=](PFrame f, bool)
    {
        //This view has changed (the interfaces in it have changed)
        //tODO handle M
        auto m = Message::fromFrame(f);
        QSet<QString> svcs;
        m_interfaces.clear();
        foreach(auto po, m->FilterPOs(bwpo::num::InterfaceDescriptor)) {
            QVariant v = MsgPack::unpack(po->contentArray());
            QVariantMap vm = v.toMap();
         //   qDebug() << "map thingy: " << vm;
            //How long is the /ifacename/iface string?
            int suffixlen = vm["prefix"].toString().length() + vm["iface"].toString().length() + 2;
            QString sname = vm["suffix"].toString();
            sname.chop(suffixlen);
            svcs.insert(sname);
            m_interfaces.append(vm);
        }
        QStringList svcL = svcs.toList();
        svcL.sort();
        if (!(svcL == m_services)) {
            m_services = svcL;
            emit servicesChanged();
        }
        emit interfacesChanged();
        qDebug() << "interfaces: " << m_interfaces;
       // qDebug() << "services:" << m_services;
    });
}

const QStringList& BWView::services()
{
    return m_services;
}

const QVariantList& BWView::interfaces()
{
    return m_interfaces;
}


