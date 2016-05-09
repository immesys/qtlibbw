#ifndef AGENTCONNECTION_H
#define AGENTCONNECTION_H

#include <QObject>
#include <QDebug>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QThread>
#include <string>
#include <functional>
#include <QJSValue>

using std::function;

class AgentConnection;

class RoutingObject
{
public:
    RoutingObject(int ronum, char *data, int length)
        : m_ronum(ronum), m_data(data), m_length(length) {}
    ~RoutingObject()
    {
        delete [] m_data;
    }
    int ronum()
    {
        return m_ronum;
    }
    const char* content ()
    {
        return m_data;
    }
    int length() {
        return m_length;
    }


private:
    int m_ronum;
    char *m_data;
    int m_length;
};

class Header
{
public:
    Header(QString key, char *data, int length) : m_key(key), m_data(data), m_length(length) {}
    Header(QString key, QString val)
    {
        m_key = key;

        QByteArray utf8 = val.toUtf8();
        m_data = new char[utf8.length()];
        m_length = utf8.length();
        memcpy(m_data,utf8.data(), m_length);
    }

    ~Header()
    {
        delete [] m_data;
    }
    const QString& key()
    {
        return m_key;
    }
    const char* content ()
    {
        return m_data;
    }
    int length() {
        return m_length;
    }
    bool asBool();
    int asInt();
    QString asString();
private:
    QString m_key;
    char* m_data;
    int m_length;
};

class PayloadObject
{
public:
    ~PayloadObject()
    {
        delete [] m_data;
    }
    static PayloadObject* load(int ponum, char* dat, int length);
    int ponum()
    {
        return m_ponum;
    }
    const char* content ()
    {
        return m_data;
    }
    int length() {
        return m_length;
    }
protected:
    PayloadObject(int ponum, char *data, int length) : m_ponum(ponum), m_data(data), m_length(length) {}
    int m_ponum;
    char *m_data;
    int m_length;
};

PayloadObject* createBasePayloadObject(int ponum, QByteArray &contents);
PayloadObject* createBasePayloadObject(int ponum, const char* dat, int length);
/*
class Status
{
public:
    Status(bool iserror, QString msg)
        : m_msg(msg), m_iserror(iserror)
    {

    }

    bool isError()
    {
        return m_iserror;
    }
    static Status okay()
    {
        return Status(false,"");
    }
    QString msg()
    {
        return m_msg;
    }

    static Status error(QString msg)
    {
        return Status(true, msg);
    }
private:
    QString m_msg;
    bool m_iserror;
};
*/
/*
template <typename ...Tz>
using Res = std::function<void(Tz...)>;
*/

template <typename ...Tz>
class Res
{
public:
    Res()
    {
        wrap = [](Tz...){};
    }
    Res(std::function<void(Tz...)> cb)
    {
        wrap = cb;
    }
    template <typename F>
    Res(F f) : Res(std::function<void(Tz...)>(f)) {}
    Res(QJSValue callback)
    {
        qDebug() << "calling res with QJS";
        if (!callback.isCallable())
        {
            qFatal("Trying to construct Res with non function JS Value");
        }
        wrap = [=](Tz... args) mutable
        {
            QJSValueList l;
            convert(l, args...);
            callback.call(l);
        };
    }
    void operator() (Tz ...args) const
    {
        wrap(args...);
    }
private:
    std::function<void(Tz...)> wrap;
};

class Frame
{
public:
    constexpr static const char* HELLO            = "helo";
    constexpr static const char* PUBLISH          = "publ";
    constexpr static const char* SUBSCRIBE        = "subs";
    constexpr static const char* PERSIST          = "pers";
    constexpr static const char* LIST             = "list";
    constexpr static const char* QUERY            = "quer";
    constexpr static const char* TAP_SUBSCRIBE    = "tsub";
    constexpr static const char* TAP_QUERY        = "tque";
    constexpr static const char* MAKE_DOT         = "makd";
    constexpr static const char* MAKE_ENTITY      = "make";
    constexpr static const char* MAKE_CHAIN       = "makc";
    constexpr static const char* BUILD_CHAIN      = "bldc";
    constexpr static const char* SET_ENTITY       = "sete";
    constexpr static const char* PUT_DOT          = "putd";
    constexpr static const char* PUT_ENTITY       = "pute";
    constexpr static const char* PUT_CHAIN        = "putc";
    constexpr static const char* ENTITY_BALANCE   = "ebal";
    constexpr static const char* ADDRESS_BALANCE  = "abal";
    constexpr static const char* BC_PARAMS        = "bcip";
    constexpr static const char* TRANSFER         = "xfer";
    constexpr static const char* MK_SHORT_ALIAS   = "mksa";
    constexpr static const char* MK_LONG_ALIAS    = "mkla";
    constexpr static const char* RESOLVE_ALIAS    = "resa";
    constexpr static const char* NEW_DRO          = "ndro";
    constexpr static const char* ACCEPT_DRO       = "adro";
    constexpr static const char* RESOLVE_REGISTRY = "rsro";
    constexpr static const char* UPDATE_SRV       = "usrv";
    constexpr static const char* LIST_DRO         = "ldro";

    constexpr static const char* RESPONSE = "resp";
    constexpr static const char* RESULT   = "rslt";

    Frame(AgentConnection *agent, const char* type, quint32 seqno)
        :agent(agent), m_seqno(seqno)
    {
        strncpy(&m_type[0],type,4);
        m_type[4] = 0;
        pos = QList<PayloadObject*>();
        headers = QList<Header*>();
        ros = QList<RoutingObject*>();
    }
    ~Frame()
    {
        qDeleteAll(pos);
        qDeleteAll(headers);
        qDeleteAll(ros);
    }
    quint32 seqno()
    {
        return m_seqno;
    }

    bool isType(const char* type)
    {
        return strcmp(m_type, type) == 0;
    }
    const char* type()
    {
        return &m_type[0];
    }

    //Returns false if not there
    bool getHeaderB(QString key, bool *valid = NULL);
    //Returns "" if not there
    QString getHeaderS(QString key, bool *valid = NULL);
    //Returns -1 if not there
    int getHeaderI(QString key, bool *valid = NULL);

    template <typename ...Tz> bool checkResponse(Res<QString,Tz...> cb, Tz ...args)
    {
        Q_ASSERT(isType(RESPONSE));
        bool ok;
        auto status = getHeaderS("status", &ok);
        Q_ASSERT(ok);
        if (status == "okay")
        {
            return true;
        }
        cb(getHeaderS("reason"), args...);
        return false;
    }

    void addPayloadObject(PayloadObject *po)
    {
        pos.append(po);
    }
    void addHeader(Header *h)
    {
        headers.append(h);
    }
    void addHeader(QString key, QString val)
    {
        Header* h = new Header(key, val);
        addHeader(h);
    }
    void addRoutingObject(RoutingObject * ro)
    {
        ros.append(ro);
    }
    void writeTo(QIODevice *o);
private:
    AgentConnection *agent;
    char m_type[5];
    const quint32 m_seqno;
    QList<PayloadObject*> pos;
    QList<RoutingObject*> ros;
    QList<Header*> headers;
};

typedef QSharedPointer<Frame> PFrame;

Q_DECLARE_METATYPE(PFrame)
Q_DECLARE_METATYPE(function<void(PFrame,bool)>)

class AgentConnection : public QObject
{
    Q_OBJECT
public:
    explicit AgentConnection(QString target, quint16 port, QObject *parent = 0)
        : QObject(parent)
    {
        qRegisterMetaType<PFrame>();
        qRegisterMetaType<function<void(PFrame,bool)>>();
        seqno = 1;
        have_received_helo = false;
        outstanding = QHash<quint32, function<void(PFrame,bool)>>();
        //All our stuff will happen on this thread
        m_thread = new QThread(this);
        m_thread->start();
        m_desthost = target;
        m_destport = port;
        moveToThread(m_thread);

     /*
        sock = new QTcpSocket(this);
        connect(sock, &QTcpSocket::connected, this, &AgentConnection::onConnect);
        connect(sock,static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
                this, &AgentConnection::onError);
        connect(sock, &QTcpSocket::readyRead, this, &AgentConnection::onArrivedData);
        sock->connectToHost(target, port);*/
    }

    bool waitForConnection()
    {
        //TODO apparently this does not work well on windows
        return sock->waitForConnected();
    }
    void beginConnection()
    {
        //We might be on another thread.
        QMetaObject::invokeMethod(this,"initSock");
    }

    void transact(PFrame f, function<void(PFrame f, bool final)> cb);
    void transact(QObject *to, PFrame f, function<void(PFrame f, bool final)> cb);
    PFrame newFrame(const char *type, quint32 seqno=0);
private:
    quint32 getSeqNo();
    void readRO(QStringList &tokens);
    void readPO(QStringList &tokens);
    void readKV(QStringList &tokens);
    QAtomicInt seqno;
    QTcpSocket *sock;
    QThread    *m_thread;
    PFrame  curFrame;
    int waitingFor;
    QHash<quint32, function<void(PFrame f, bool final)>> outstanding;
    void onArrivedFrame(PFrame f);
    bool have_received_helo;
    QString m_desthost;
    qint16 m_destport;
private slots:
    void onConnect();
    void onError();
    void onArrivedData();
    void initSock();
    void doTransact(PFrame f, function<void(PFrame,bool)> cb);
signals:
    void agentChanged(bool connected, QString msg);

};

#endif // AGENTCONNECTION_H
