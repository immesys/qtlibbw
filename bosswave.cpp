#include "bosswave.h"

#include "allocations.h"

#include <QFile>
#include <QQmlEngine>

#include <msgpack.h>

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
//TODO add parameters
void BW::publish(QString uri, QList<PayloadObject*> poz, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::PUBLISH);
    foreach(auto po, poz)
    {
        f->addPayloadObject(po);
    }
    f->addHeader("autochain","true");
    f->addHeader("uri",uri);
    f->addHeader("doverify","true");
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if(f->checkResponse(on_done))
        {
            on_done("");
        }
    });
}

void BW::publishMsgPack(QString uri, QString PODF, QVariantMap val, Res<QString> on_done)
{
    publishMsgPack(uri, BW::fromDF(PODF), val, on_done);
}
void BW::publishMsgPack(QString uri, int ponum, QVariantMap val, Res<QString> on_done)
{
    //qDebug() << "we got called, da" << (on_done == _nop_res_status);
    QByteArray contents = MsgPack::pack(val);
    PayloadObject* po = createBasePayloadObject(ponum,contents.data(),contents.length());
    publish(uri,{po}, on_done);
//    QByteArray contents = MsgPack::pack(val);
  //  qDebug() << "it worked2: " << contents;
}
void BW::publishMsgPack(QString uri, int PONum, QVariantMap val, QJSValue on_done)
{
    publishMsgPack(uri, PONum, val, Res<QString>(on_done));
}
void BW::publishMsgPack(QString uri, QString PODF, QVariantMap val, QJSValue on_done)
{
    publishMsgPack(uri, PODF, val, Res<QString>(on_done));
}

void BW::publishText(QString uri, QString msg, Res<QString> on_done)
{
    publishText(uri, bwpo::num::Text, msg, on_done);
}
void BW::publishText(QString uri, QString msg, QJSValue on_done)
{
    publishText(uri, bwpo::num::Text, msg, Res<QString>(on_done));
}

void BW::publishText(QString uri, int PONum, QString msg, Res<QString> on_done)
{
    QByteArray encoded = msg.toUtf8();
    PayloadObject* po = createBasePayloadObject(PONum, encoded);
    publish(uri, {po}, on_done);
}

void BW::publishText(QString uri, int PONum, QString msg, QJSValue on_done)
{
    publishText(uri, PONum, msg, Res<QString>(on_done));
}


void BW::query(QString uri, Res<QString, QList<PMessage> > on_done)
{
    auto f = agent()->newFrame(Frame::QUERY);
    auto lst = new QList<PMessage>();
    f->addHeader("autochain","true");
    f->addHeader("uri", uri);
    f->addHeader("doverify","true");
    f->addHeader("unpack", "true");
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

void BW::subscribe(QString uri, Res<PMessage> on_msg, Res<QString> on_done)
{
    auto f = agent()->newFrame(Frame::SUBSCRIBE);
    f->addHeader("autochain","true");
    f->addHeader("uri",uri);
    f->addHeader("doverify","true");
    f->addHeader("unpack","true");
    agent()->transact(this, f, [=](PFrame f, bool)
    {
        if (f->isType(Frame::RESPONSE))
        {
            if(f->checkResponse(on_done))
            {
                qDebug() << "invoking nil reply";
                on_done("");
            }
            else {
                qDebug() << "not invoking nil reply";
            }
        }
        else
        {
            on_msg(Message::fromFrame(f));
        }
    });
}

void BW::subscribeMsgPack(QString uri, Res<QVariantMap> on_msg, Res<QString> on_done)
{
    qDebug() <<"fun3";
    BW::subscribe(uri, [=](PMessage m)
    {
        foreach(auto po, m->FilterPOs(bwpo::num::MsgPack, bwpo::mask::MsgPack))
        {
            QVariant v = MsgPack::unpack(po->contentArray());
            on_msg(v.toMap());
        }
    }, on_done);
}

void BW::subscribeMsgPack(QString uri, QJSValue on_msg)
{
    qDebug() <<"fun2";
    subscribeMsgPack(uri, ERes<QVariantMap>(on_msg));
}

void BW::subscribeMsgPack(QString uri, QJSValue on_msg, QJSValue on_done)
{
    qDebug() <<"fun1";
    subscribeMsgPack(uri, ERes<QVariantMap>(on_msg), ERes<QString>(on_done));
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


