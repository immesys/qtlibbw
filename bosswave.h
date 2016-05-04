#ifndef BOSSWAVE_H
#define BOSSWAVE_H

#include <QQuickItem>

class RetVal : public QObject
{
    Q_OBJECT
public:
  //  Q_PROPERTY(QString val READ getVal)

    RetVal(QString s, QObject *parent) : QObject(parent)
    {
        this->s = s;
    }
    ~RetVal()
    {
        qDebug() << "retval deconstructed";
    }

    const QString getVal() {
        return s;
    }
    QString s;
};

class BW : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BW)

public:
    BW(QObject *parent = 0);
    ~BW();

    // This is used by the QML engine to instantiate the bosswave singleton
    static QObject *qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine);

    // This can be used to obtain references to the BW object
    static BW *instance();

    // Set the agent to the given host and port. Any existing connection will
    // be torn down. Any existing subscriptions / views will not be recreated
    // and the entity must be set again.
    //void setAgent(QString &host, quint16 port);

    RetVal* foo(QObject *caller)
    {
        return new RetVal(QString("hello"), caller);
    }

    /**
     * @brief Set the entity
     * @param filename a BW entity file to use
     */
    //void setEntityFile(QString *filename);

    /**
     * @brief Set the entity
     * @param contents the binary contents of an entity descriptor
     *
     * Note that if read from an entity file, the first byte of the file
     * must be stripped as it is an RO type indicator.
     *
     * @see setEntityFile
     */
    //void setEntity(QByteArray &contents);

    /**
     * @brief Accept a DRO
     * @param account the account to bankroll the operation
     * @param vk the verifying key to accept an offer from
     * @param ns the namespace that is accepting an offer
     */
    //void acceptDesignatedRouterOffer(int account, QString &vk, Entity &ns);
    /**
     * @brief Accept a DRO with current Entity
     * @param account the account to bankroll the operation
     * @param vk the verifying key to accept an offer from
     */
    //void acceptDesignatedRouterOffer(int account, QString &vk);



private:
    QQmlEngine *engine;
    QJSEngine *jsengine;
};

#endif // BOSSWAVE_H


