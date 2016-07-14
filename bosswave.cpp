#include "bosswave.h"

#include "allocations.h"

#include <QFile>
#include <QQmlEngine>

#include <msgpack.h>

#include <cstdio>

class NotImplementedException : public std::exception
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

class BadRouterMessageException : public std::exception
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
    _nop_res_status = [](QString) {};
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

void BW::createEntity(QDateTime expiry, qreal expiryDelta, QString contact,
                      QString comment, QList<QString> revokers, bool omitCreationDate,
                      Res<QString, QString, QByteArray> on_done)
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
        if (f->checkResponse(on_done, QStringLiteral(""), QByteArray()))
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
                   Res<QString, QString, QByteArray> on_done)
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
        if (f->checkResponse(on_done, QStringLiteral(""), QByteArray()))
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

void BW::createDOTChain(QList<QString> dots, bool isPermission, bool unElaborate,
                        Res<QString, QString, RoutingObject*> on_done)
{
    auto f = agent()->newFrame(Frame::MAKE_CHAIN);
    f->addHeader("ispermission", isPermission ? "true" : "false");
    f->addHeader("unelaborate", unElaborate ? "true" : "false");
    for (auto i = dots.begin(); i != dots.end(); i++)
    {
        f->addHeader("dot", *i);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), (RoutingObject*) nullptr))
        {
            QList<RoutingObject*> ros = f->getRoutingObjects();
            if (ros.length() != 1)
            {
                on_done("bad response", "", nullptr);
                return;
            }
            QString hash = f->getHeaderS("hash");
            RoutingObject* ro = ros[0];

            on_done("", hash, ro);
        }
    });
}

void BW::publish(QString uri, QString primaryAccessChain, bool autoChain,
                 QList<RoutingObject*> roz, QList<PayloadObject*> poz,
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

    foreach (auto ro, roz)
    {
        f->addRoutingObject(ro);
    }

    foreach (auto po, poz)
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


void BW::publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                        int ponum, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        Res<QString> on_done)
{
    QByteArray contents = MsgPack::pack(val);
    PayloadObject* po = createBasePayloadObject(ponum, contents.data(), contents.length());
    publish(uri, primaryAccessChain, autoChain, roz, {po}, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, on_done);
}

void BW::publishMsgPack(QVariantMap params, QJSValue on_done)
{
    QString uri = params["URI"].toString();
    QString primaryAccessChain = params["PrimaryAccessChain"].toString();
    bool autoChain = params["AutoChain"].toBool();
    QList<RoutingObject*> roz;
    QVariantMap payload = params["Payload"].toMap();
    int ponum = bwpo::num::MsgPack;
    QDateTime expiry = params["Expiry"].toDateTime();
    qreal expiryDelta = -1.0;
    QString elaboratePAC = params["ElaboratePAC"].toString();
    bool doNotVerify = params["DoNotVerify"].toBool();
    bool persist = params["Persist"].toBool();

    if (params.contains("RoutingObjects"))
    {
        QVariantList ros = params["RoutingObjects"].toList();
        for (auto i = ros.begin(); i != ros.end(); i++)
        {
            roz.append(i->value<RoutingObject*>());
        }
    }

    if (params.contains("PONum"))
    {
        ponum = params["PONum"].toInt();
    }

    if (params.contains("ExpiryDelta"))
    {
        expiryDelta = params["ExpiryDelta"].toReal();
    }

    this->publishMsgPack(uri, primaryAccessChain, autoChain, roz, ponum,
                         payload, expiry, expiryDelta, elaboratePAC,
                         doNotVerify, persist, ERes<QString>(on_done));
}

void BW::publishText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                     int PONum, QString msg, QDateTime expiry, qreal expiryDelta,
                     QString elaboratePAC, bool doNotVerify, bool persist,
                     Res<QString> on_done)
{
    QByteArray encoded = msg.toUtf8();
    PayloadObject* po = createBasePayloadObject(PONum, encoded);
    publish(uri, primaryAccessChain, autoChain, roz, {po}, expiry, expiryDelta, elaboratePAC, doNotVerify, persist, on_done);
}

void BW::publishText(QVariantMap params, QJSValue on_done)
{
    QString uri = params["URI"].toString();
    QString primaryAccessChain = params["PrimaryAccessChain"].toString();
    bool autoChain = params["AutoChain"].toBool();
    QList<RoutingObject*> roz;
    QString payload = params["Payload"].toString();
    int ponum = bwpo::num::Text;
    QDateTime expiry = params["Expiry"].toDateTime();
    qreal expiryDelta = -1.0;
    QString elaboratePAC = params["ElaboratePAC"].toString();
    bool doNotVerify = params["DoNotVerify"].toBool();
    bool persist = params["Persist"].toBool();

    if (params.contains("RoutingObjects"))
    {
        QVariantList ros = params["RoutingObjects"].toList();
        for (auto i = ros.begin(); i != ros.end(); i++)
        {
            roz.append(i->value<RoutingObject*>());
        }
    }

    if (params.contains("PONum"))
    {
        ponum = params["PONum"].toInt();
    }

    if (params.contains("ExpiryDelta"))
    {
        expiryDelta = params["ExpiryDelta"].toReal();
    }

    this->publishText(uri, primaryAccessChain, autoChain, roz, ponum,
                      payload, expiry, expiryDelta, elaboratePAC,
                      doNotVerify, persist, ERes<QString>(on_done));
}

void BW::subscribe(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
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
    for (auto i = roz.begin(); i != roz.end(); i++)
    {
        f->addRoutingObject(*i);
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

void BW::subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, bool leavePacked, Res<int, QVariantMap> on_msg,
                          Res<QString> on_done, Res<QString> on_handle)
{
    BW::subscribe(uri, primaryAccessChain, autoChain, roz, expiry,
                  expiryDelta, elaboratePAC, doNotVerify, leavePacked,
                  [=](PMessage m)
    {
        foreach(auto po, m->FilterPOs(bwpo::num::MsgPack, bwpo::mask::MsgPack))
        {
            QVariant v = MsgPack::unpack(po->contentArray());
            on_msg(po->ponum(), v.toMap());
        }
    }, on_done, on_handle);
}

void BW::subscribeMsgPack(QVariantMap params, QJSValue on_msg, QJSValue on_done, QJSValue on_handle)
{
    QString uri = params["URI"].toString();
    QString primaryAccessChain = params["PrimaryAccessChain"].toString();
    bool autoChain = params["AutoChain"].toBool();
    QList<RoutingObject*> roz;
    QDateTime expiry = params["Expiry"].toDateTime();
    qreal expiryDelta = -1.0;
    QString elaboratePAC = params["ElaboratePAC"].toString();
    bool doNotVerify = params["DoNotVerify"].toBool();
    bool persist = params["Persist"].toBool();

    if (params.contains("RoutingObjects"))
    {
        QVariantList ros = params["RoutingObjects"].toList();
        for (auto i = ros.begin(); i != ros.end(); i++)
        {
            roz.append(i->value<RoutingObject*>());
        }
    }

    if (params.contains("ExpiryDelta"))
    {
        expiryDelta = params["ExpiryDelta"].toReal();
    }

    this->subscribeMsgPack(uri, primaryAccessChain, autoChain, roz, expiry,
                           expiryDelta, elaboratePAC, doNotVerify, persist,
                           ERes<int, QVariantMap>(on_msg),
                           ERes<QString>(on_done),
                           ERes<QString>(on_handle));
}

void BW::subscribeText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                       QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                       bool doNotVerify, bool leavePacked, Res<int, QString> on_msg,
                       Res<QString> on_done, Res<QString> on_handle)
{
    BW::subscribe(uri, primaryAccessChain, autoChain, roz, expiry,
                  expiryDelta, elaboratePAC, doNotVerify, leavePacked,
                  [=](PMessage m)
    {
        foreach(auto po, m->FilterPOs(bwpo::num::Text, bwpo::mask::Text))
        {
            on_msg(po->ponum(), QString(po->contentArray()));
        }
    }, on_done, on_handle);
}

void BW::subscribeText(QVariantMap params, QJSValue on_msg, QJSValue on_done, QJSValue on_handle)
{
    QString uri = params["URI"].toString();
    QString primaryAccessChain = params["PrimaryAccessChain"].toString();
    bool autoChain = params["AutoChain"].toBool();
    QList<RoutingObject*> roz;
    QDateTime expiry = params["Expiry"].toDateTime();
    qreal expiryDelta = -1.0;
    QString elaboratePAC = params["ElaboratePAC"].toString();
    bool doNotVerify = params["DoNotVerify"].toBool();
    bool persist = params["Persist"].toBool();

    if (params.contains("RoutingObjects"))
    {
        QVariantList ros = params["RoutingObjects"].toList();
        for (auto i = ros.begin(); i != ros.end(); i++)
        {
            roz.append(i->value<RoutingObject*>());
        }
    }

    if (params.contains("ExpiryDelta"))
    {
        expiryDelta = params["ExpiryDelta"].toReal();
    }

    this->subscribeText(uri, primaryAccessChain, autoChain, roz, expiry,
                        expiryDelta, elaboratePAC, doNotVerify, persist,
                        ERes<int, QString>(on_msg),
                        ERes<QString>(on_done),
                        ERes<QString>(on_handle));
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

void BW::setEntityFile(QString filename, Res<QString, QString> on_done)
{
    QFile f(filename);
    qDebug() << "the filename is" << f.fileName();
    if (!f.open(QIODevice::ReadOnly))
    {
        on_done(QString("Could not open entity file: %1").arg(f.errorString()), QStringLiteral(""));
        return;
    }
    QByteArray contents = f.readAll().mid(1);
    setEntity(contents, on_done);
}


void BW::setEntityFromEnviron(Res<QString, QString> on_done)
{
    QByteArray a = qgetenv("BW2_DEFAULT_ENTITY");
    if (a.isEmpty()) {
        on_done("BW2_DEFAULT_ENTITY not set", QStringLiteral(""));
        return;
    }
    setEntityFile(a.data(), on_done);
}

void BW::setEntity(QByteArray keyfile, Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::SET_ENTITY);
    auto po = createBasePayloadObject(bwpo::num::ROEntityWKey, keyfile);
    f->addPayloadObject(po);
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if(f->checkResponse(on_done, QStringLiteral("")))
        {
            this->m_vk = f->getHeaderS("vk");
            on_done("", this->m_vk);
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

void BW::query(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
               QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
               bool doNotVerify, bool leavePacked,
               Res<QString, PMessage, bool> on_result)
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
    for (auto i = roz.begin(); i != roz.end(); i++)
    {
        f->addRoutingObject(*i);
    }
    f->addHeader("elaborate_pac", elaboratePAC);
    if (!leavePacked)
    {
        f->addHeader("unpack", "true");
    }
    f->addHeader("doverify", doNotVerify ? "false": "true");

    agent()->transact(this, f, [=](PFrame f, bool final)
    {
        if (f->isType(Frame::RESPONSE))
        {
            if (!f->checkResponse(on_result, PMessage(), final))
            {
                return;
            }
        }

        bool ok;
        f->getHeaderS("from", &ok);
        if (ok)
        {
            on_result("", Message::fromFrame(f), final);
        }
    });
}

struct queryliststate
{
    QList<PMessage> messages;
    bool goterror;
};

void BW::queryList(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked,
                   Res<QString, QList<PMessage>> on_done)
{

    struct queryliststate* state = new struct queryliststate;
    state->goterror = false;
    this->query(uri, primaryAccessChain, autoChain, roz, expiry, expiryDelta, elaboratePAC,
                doNotVerify, leavePacked, [=](QString error, PMessage msg, bool final)
    {
        if (state->goterror)
        {
            goto end;
        }

        if (error.length() != 0)
        {
            on_done(error, state->messages);
            state->goterror = true;
            goto end;
        }

        state->messages.append(msg);

    end:
        if (final)
        {
            if (!state->goterror)
            {
                on_done("", state->messages);
            }
            delete state;
        }
    });
}

void BW::queryOne(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                  QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                  bool doNotVerify, bool leavePacked, Res<QString, PMessage> on_done)
{
    bool* fired = new bool;
    *fired = false;

    this->query(uri, primaryAccessChain, autoChain, roz, expiry, expiryDelta, elaboratePAC,
                doNotVerify, leavePacked, [=](QString error, PMessage msg, bool final)
    {
        if (!*fired)
        {
            on_done(error, msg);
            *fired = true;
        }

        if (final)
        {
            delete fired;
        }
    });
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
    MetadataTuple metadata(val, QDateTime::currentMSecsSinceEpoch() * Q_INT64_C(1000000));

    if (uri.endsWith(QStringLiteral("/")))
    {
        uri.chop(1);
    }
    uri += "/!meta/";
    uri += key;

    this->publishMsgPack(uri, "", true, QList<RoutingObject*>(), bwpo::num::SMetadata, metadata.toVariantMap(), QDateTime(), -1, "", false, true, on_done);
}

void BW::delMetadata(QString uri, QString key, Res<QString> on_done)
{
    if (uri.endsWith(QStringLiteral("/")))
    {
        uri.chop(1);
    }
    uri += "/!meta/";
    uri += key;

    this->publish(uri, "", true, QList<RoutingObject*>(), QList<PayloadObject*>(), QDateTime(), -1, "", false, true, on_done);
}

struct metadata_kv
{
    QString k;
    MetadataTuple m;
    QString o;
};

struct metadata_info
{
    QVector<struct metadata_kv> chans;
    int numreturned;
    int errorhappened;
};

void BW::getMetadata(QString uri, Res<QString, QMap<QString, MetadataTuple>, QMap<QString, QString>> on_done)
{
    QStringList parts = uri.split('/', QString::SkipEmptyParts);
    QString turi("");

    struct metadata_info* mi = new struct metadata_info;
    mi->chans.resize(parts.size());
    mi->numreturned = 0;
    mi->errorhappened = false;

    int li = 0;
    for (auto i = parts.begin(); i != parts.end(); i++)
    {
        turi += *i;
        turi += "/";

        QString touse(turi);
        touse += "!meta/";

        this->queryList(touse, "", true, QList<RoutingObject*>(), QDateTime(), -1, "",
                        false, false, [=](QString error, QList<PMessage> messages)
        {

            if (error.length() != 0)
            {
                on_done(error, QMap<QString, MetadataTuple>(), QMap<QString, QString>());
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
                        metadata.m = MetadataTuple(dict);
                    }
                }
            }

            if (++mi->numreturned == mi->chans.length())
            {
                if (!mi->errorhappened)
                {
                    /* Call on_done. */
                    QMap<QString, QString> rvO;
                    QMap<QString, MetadataTuple> rvM;

                    for (auto res = mi->chans.begin(); res != mi->chans.end(); res++)
                    {
                        rvO[res->k] = res->o;
                        rvM[res->k] = res->m;
                    }

                    on_done("", rvM, rvO);
                }

                delete mi;
            }
        });

        li++;
    }
}

void BW::getMetadataKey(QString uri, QString key, Res<QString, MetadataTuple, QString> on_done)
{
    if (key.length() == 0)
    {
        on_done("Key cannot be the empty string", MetadataTuple(), "");
        return;
    }

    QStringList parts = uri.split('/', QString::SkipEmptyParts);
    QString turi("");

    struct metadata_info* mi = new struct metadata_info;
    mi->chans.resize(parts.size());
    mi->numreturned = 0;
    mi->errorhappened = false;

    int li = 0;
    for (auto i = parts.begin(); i != parts.end(); i++)
    {
        turi += *i;
        turi += "/";

        QString touse(turi);
        touse += "!meta/";
        touse += key;

        this->queryOne(touse, "", true, QList<RoutingObject*>(), QDateTime(), -1, "",
                       false, false, [=](QString error, PMessage message)
        {

            if (error.length() != 0)
            {
                on_done(error, MetadataTuple(), "");
                mi->errorhappened = true;
            }
            else
            {
                struct metadata_kv& metadata = mi->chans[li];

                metadata.k = key;
                metadata.o = turi;
                QList<PayloadObject*> pos = message->FilterPOs(bwpo::num::SMetadata, bwpo::mask::SMetadata);
                for (auto k = pos.begin(); k != pos.end(); k++)
                {
                    QVariant dictv = MsgPack::unpack((*k)->contentArray());
                    QVariantMap dict = dictv.toMap();
                    metadata.m = MetadataTuple(dict);
                }
            }

            if (++mi->numreturned == mi->chans.length())
            {
                if (!mi->errorhappened)
                {
                    /* Call on_done. */

                    for (auto res = mi->chans.rbegin(); res != mi->chans.rend(); res++)
                    {
                        if (res->k.length() != 0)
                        {
                            on_done("", res->m, res->o);
                            goto end;
                        }
                    }
                    on_done("", MetadataTuple(), "");
                }
        end:
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

void BW::entityBalances(Res<QString, QVector<struct balanceinfo>> on_done)
{
    auto f = agent()->newFrame(Frame::ENTITY_BALANCE);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QVector<struct balanceinfo>()))
        {
            QVector<struct balanceinfo> rv;
            QList<PayloadObject*> pos = f->getPayloadObjects();
            for (auto i = pos.begin(); i != pos.end(); i++)
            {
                PayloadObject* po = *i;
                if (po->ponum() == bwpo::num::AccountBalance)
                {
                    QList<QByteArray> parts = po->contentArray().split(',');
                    struct balanceinfo bi;
                    bi.addr = QString(parts[0]);
                    bi.decimal = QString(parts[1]);
                    bi.human = QString(parts[2]);
                    bi.value = (qreal) parts[3].toDouble();
                    rv.push_back(bi);
                }
            }
            on_done("", rv);
        }
    });
}

void BW::addressBalance(QString addr, Res<QString, struct balanceinfo> on_done)
{
    if (addr.mid(0, 2) == QStringLiteral("0x"))
    {
        addr = addr.mid(2, -1);
    }

    struct balanceinfo zero = { QStringLiteral(""),
                QStringLiteral(""),QStringLiteral(""), 0.0 };

    if (addr.length() != 40)
    {
        on_done(QStringLiteral("Address must be 40 hex characters"), zero);
    }

    auto f = agent()->newFrame(Frame::ENTITY_BALANCE);
    f->addHeader("address", addr);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, zero))
        {
            QList<PayloadObject*> pos = f->getPayloadObjects();
            if (pos.length() == 0)
            {
                throw BadRouterMessageException("At least one PO expected on addressBalance command", "0");
            }
            PayloadObject* po = pos[0];
            QList<QByteArray> parts = po->contentArray().split(',');

            struct balanceinfo bi;
            bi.addr = QString(parts[0]);
            bi.decimal = QString(parts[1]);
            bi.human = QString(parts[2]);
            bi.value = (qreal) parts[3].toDouble();

            on_done("", bi);
        }
    });
}

void BW::getBCInteractionParams(Res<QString, struct currbcip> on_done)
{
    this->setBCInteractionParams(-1, -1, -1, on_done);
}

void BW::setBCInteractionParams(int64_t confirmations, int64_t timeout, int64_t maxAge, Res<QString, struct currbcip> on_done)
{
    auto f = agent()->newFrame(Frame::BC_PARAMS);

    if (confirmations >= 0)
    {
        f->addHeader("confirmations", QString::number(confirmations));
    }
    if (timeout >= 0)
    {
        f->addHeader("timeout", QString::number(timeout));
    }
    if (maxAge >= 0)
    {
        f->addHeader("maxage", QString::number(maxAge));
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        struct currbcip rv = { Q_INT64_C(0), Q_INT64_C(0), Q_INT64_C(0),
                             Q_INT64_C(0), Q_UINT64_C(0), Q_INT64_C(0),
                             Q_INT64_C(0), Q_INT64_C(0) };
        if (f->checkResponse(on_done, rv))
        {
            rv.confirmations = f->getHeaderS("confirmations").toLongLong();
            rv.timeout = f->getHeaderS("timeout").toLongLong();
            rv.maxAge = f->getHeaderS("maxage").toLongLong();
            rv.currentBlock = f->getHeaderS("currentblock").toULongLong();
            rv.currentAge = f->getHeaderS("currentage").toLongLong();
            rv.peers = f->getHeaderS("peers").toLongLong();
            rv.highestBlock = f->getHeaderS("highest").toLongLong();
            rv.difficulty = f->getHeaderS("difficulty").toLongLong();
            on_done("", rv);
        }
    });
}

void BW::transferEther(int from, QString to, double ether, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::TRANSFER);

    f->addHeader("account", QString::number(from));
    f->addHeader("address", to);
    f->addHeader("valuewei", QString::number(ether * 1e18, 'f', 0));

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::newDesignatedRouterOffer(int account, QString nsvk, Entity* dr, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::NEW_DRO);
    f->addHeader("account", QString::number(account));
    f->addHeader("nsvk", nsvk);
    if (dr != nullptr)
    {
        QByteArray sblob = dr->getSigningBlob();
        PayloadObject* po = PayloadObject::load(bwpo::num::ROEntityWKey, sblob.data(), sblob.length());
        f->addPayloadObject(po);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::revokeDesignatedRouterOffer(int account, QString nsvk, Entity* dr, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::REVOKE_DRO);
    f->addHeader("account", QString::number(account));
    f->addHeader("nsvk", nsvk);
    if (dr != nullptr)
    {
        QByteArray sblob = dr->getSigningBlob();
        PayloadObject* po = PayloadObject::load(bwpo::num::ROEntityWKey, sblob.data(), sblob.length());
        f->addPayloadObject(po);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::revokeAcceptanceOfDesignatedRouterOffer(int account, QString drvk, Entity* dr, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::REVOKE_DRO_ACCEPT);
    f->addHeader("account", QString::number(account));
    f->addHeader("drvk", drvk);
    if (dr != nullptr)
    {
        QByteArray sblob = dr->getSigningBlob();
        PayloadObject* po = PayloadObject::load(bwpo::num::ROEntityWKey, sblob.data(), sblob.length());
        f->addPayloadObject(po);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::revokeEntity(QString vk, Res<QString, QString, QByteArray> on_done)
{
    auto f = agent()->newFrame(Frame::REVOKE_RO);
    f->addHeader("entity", vk);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), QByteArray()))
        {
            QString hash = f->getHeaderS("hash");
            QList<PayloadObject*> pos = f->getPayloadObjects();
            if (pos.length() == 0)
            {
                throw BadRouterMessageException("At least one PO expected on revokeEntity command", "0");
            }
            PayloadObject* po = pos[0];
            on_done("", hash, po->contentArray());
        }
    });
}

void BW::revokeDOT(QString hash, Res<QString, QString, QByteArray> on_done)
{
    auto f = agent()->newFrame(Frame::REVOKE_RO);
    f->addHeader("dot", hash);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done, QStringLiteral(""), QByteArray()))
        {
            QString hash = f->getHeaderS("hash");
            QList<PayloadObject*> pos = f->getPayloadObjects();
            if (pos.length() == 0)
            {
                throw BadRouterMessageException("At least one PO expected on revokeDOT command", "0");
            }
            PayloadObject* po = pos[0];
            on_done("", hash, po->contentArray());
        }
    });
}

void BW::publishRevocation(int account, QByteArray blob, Res<QString, QString> on_done)
{
    auto f = agent()->newFrame(Frame::PUT_REVOCATION);
    PayloadObject* po = PayloadObject::load(bwpo::num::RORevocation, blob.data(), blob.length());
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

void BW::getDesignatedRouterOffers(QString nsvk, Res<QString, QString, QString, QList<QString>> on_done)
{
    auto f = agent()->newFrame(Frame::LIST_DRO);
    f->addHeader("nsvk", nsvk);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        QList<QString> rv;
        if (f->checkResponse(on_done, QStringLiteral(""), QStringLiteral(""), rv))
        {
            auto pos = f->getPayloadObjects();
            for (auto i = pos.begin(); i != pos.end(); i++)
            {
                PayloadObject* po = *i;
                if (po->ponum() == bwpo::num::RODRVK)
                {
                    rv.append(po->contentArray().toBase64(QByteArray::Base64UrlEncoding));
                }
            }
            on_done("", f->getHeaderS("active"), f->getHeaderS("srv"), rv);
        }
    });
}

void BW::acceptDesignatedRouterOffer(int account, QString drvk, Entity* ns, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::ACCEPT_DRO);
    f->addHeader("account", QString::number(account));
    f->addHeader("drvk", drvk);
    if (ns != nullptr)
    {
        QByteArray sblob = ns->getSigningBlob();
        PayloadObject* po = PayloadObject::load(bwpo::num::ROEntityWKey, sblob.data(), sblob.length());
        f->addPayloadObject(po);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::setDesignatedRouterSRVRecord(int account, QString srv, Entity* dr, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::UPDATE_SRV);
    f->addHeader("account", QString::number(account));
    f->addHeader("srv", srv);
    if (dr != nullptr)
    {
        QByteArray sblob = dr->getSigningBlob();
        PayloadObject* po = PayloadObject::load(bwpo::num::ROEntityWKey, sblob.data(), sblob.length());
        f->addPayloadObject(po);
    }

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::createLongAlias(int account, QByteArray key, QByteArray val, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::MK_LONG_ALIAS);
    f->addHeader("account", QString::number(account));
    f->addHeaderB("content", val);
    f->addHeaderB("key", key);

    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->checkResponse(on_done))
        {
            on_done("");
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


