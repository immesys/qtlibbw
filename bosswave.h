#ifndef BOSSWAVE_H
#define BOSSWAVE_H

#include <QDateTime>
#include <QQuickItem>
#include <QTimer>
#include <QJSValueList>
#include <QSharedPointer>

#include "utils.h"
#include "agentconnection.h"
#include "message.h"

QT_FORWARD_DECLARE_CLASS(MetadataTuple)
QT_FORWARD_DECLARE_CLASS(BalanceInfo)
QT_FORWARD_DECLARE_CLASS(SimpleChain)
QT_FORWARD_DECLARE_CLASS(BWView)

const QString elaborateDefault("");
const QString elaborateFull("full");
const QString elaboratePartial("partial");
const QString elaborateNone("none");


/*! \mainpage BOSSWAVE Wavelet Viewer
 *
 * \section intro_sec Introduction
 *
 * This contains the documentation for the BOSSWAVE Wavelet API.
 *
 * The functions are grouped into two modules, those accessible only from C++,
 * and those accessible from both C++ and QML.
 *
 * To view the documentation, click the "Wavelet API overview" tab above.
 */

/**
 * \defgroup cpp C++ API
 * \brief Functions for interacting with BOSSWAVE from C++ code
 *
 * These functions are not accessible from Wavelets, but for programs that
 * are fully standalone (compiled against libqtbw) they can be used to obtain
 * functionality not found in the QML API. All the QML functions can also be
 * invoked from C++.
 */

/**
 * \defgroup qml QML API
 * \brief Functions accessible to Wavelets
 *
 * These functions can be used in Wavelets by importing the BOSSWAVE module:
 * \code{.qml}
 * import BOSSWAVE 1.0
 *
 * Button {
 *   onClicked:
 *   {
 *     BW.publishMsgPack("my/url", "2.0.7.2", {"key":"value"},
 *       function(status) {
 *         console.log("publish status:", status);
 *       });
 *   }
 * }
 * \endcode
 *
 * They can also be used in C++ via normal function invocations.
 */

class BW : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BW)

public:
    BW(QObject *parent = 0);
    ~BW();

    enum RegistryValidity
    {
        StateUnknown = 0,
        StateValid = 1,
        StateExpired = 2,
        StateRevoked = 3,
        StateError = 4
    };
    Q_ENUM(RegistryValidity)

    // This is used by the QML engine to instantiate the bosswave singleton
    static QObject *qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine);

    /**
     * @brief This can be used to obtain references to the BW object
     * @return The BW2 singleton instance
     *
     * @ingroup cpp
     * @since 1.0
     */
    static BW *instance();

    /**
     * @brief Get the integer PO Number from dot form
     * @param df The dot form, e.g. 2.0.3.1
     * @return The integer PONum
     *
     * @ingroup cpp
     * @since 1.0
     */
    static int fromDF(QString df);

    /**
     * @brief Connect to a BOSSWAVE agent
     * @param host The IP address/hostname to connect to
     * @param port The port (generally 28589)
     *
     * Set the agent to the given host and port. Any existing connection will
     * be torn down. Any existing subscriptions / views will not be recreated
     * and the entity must be set again. agentConnected() will be signalled
     * when this process is complete
     *
     * @ingroup cpp
     * @since 1.0
     */
    void connectAgent(QString host, quint16 port);

    /**
     * @brief createEntity Create a new entity
     * @param expiry The time at which the entity expires. Ignored if invalid (year == 0)
     * @param expiryDelta The number of milliseconds after the current time at which the entity expires
     * @param contact Contact info
     * @param comment Comment
     * @param revokers List of revoker VKs
     * @param omitCreationDate If true, the creation date is omitted
     * @param on_done Callback that takes as arguments (1) the error message (empty string if none), (2) the VK of the new entity, and (3) binary representation
     */
    Q_INVOKABLE void createEntity(QDateTime expiry, qreal expiryDelta, QString contact,
                                  QString comment, QList<QString> revokers, bool omitCreationDate,
                                  Res<QString, QString, QByteArray> on_done);

    /**
     * @brief createDOT Create a Declaration of Trust (DOT)
     * @param isPermission True if this DOT is a permission DOT (which are not yet implemented)
     * @param to The entity to which the DOT is created
     * @param ttl The time-to-live (TTL) of this DOT
     * @param expiry The time at which the entity expires. Ignored if invalid (year == 0)
     * @param expiryDelta The number of milliseconds after the current time at which the entity expires
     * @param contact Contact info
     * @param comment Comment
     * @param revokers List of revokers
     * @param omitCreationDate If true, the creation date is omitted
     * @param uri The URI to which permissions are granted
     * @param accessPermissions String describing the permissions granted
     * @param appPermissions Used for permission DOTs
     * @param on_done Callback that takes are arguments (1) the error message (empty string if none), (2) the hash of the new dot, and (3) binary representation
     */
    Q_INVOKABLE void createDOT(bool isPermission, QString to, unsigned int ttl, QDateTime expiry,
                               qreal expiryDelta, QString contact, QString comment,
                               QList<QString> revokers, bool omitCreationDate, QString uri,
                               QString accessPermissions, QVariantMap appPermissions,
                               Res<QString, QString, QByteArray> on_done);

    void createDOTChain(QList<QString> dots, bool isPermission, bool unElaborate,
                        Res<QString, QString, RoutingObject*> on_done);

    /**
     * @brief publish a bosswave message
     * @param uri the URI to publish to
     * @param poz the payload objects to publish
     * @param on_done a function to call when the operation is complete.
     *
     * @ingroup cpp
     * @since 1.0
     */
    void publish(QString uri, QString primaryAccessChain, bool autoChain,
                 QList<RoutingObject*> roz, QList<PayloadObject*> poz,
                 QDateTime expiry, qreal expiryDelta, QString elaboratePAC, bool doNotVerify,
                 bool persist, Res<QString> on_done = _nop_res_status);


    /**
     * @brief Publish a MsgPack object to the given URI
     *
     * @ingroup qml
     * @since 1.0
     */
    Q_INVOKABLE void publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                                    int ponum, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                                    QString elaboratePAC, bool doNotVerify, bool persist,
                                    Res<QString> on_done = _nop_res_status);

    /**
      * @brief Publish text of the specified type to the given URI
      *
      * @ingroup qml
      * @since 1.3
      */
    Q_INVOKABLE void publishText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                                 int ponum, QString msg, QDateTime expiry, qreal expiryDelta,
                                 QString elaboratePAC, bool doNotVerify, bool persist,
                                 Res<QString> on_done = _nop_res_status);

    /**
     * @brief Subscribe to the given URI
     * @param uri The URI to subscribe to
     * @param on_msg A callback for each received message
     * @param on_done The callback to be executed with the error message. Empty implies success.
     * @param on_handle The callback to be executed with the subscribe handle.
     *
     * @ingroup cpp
     * @since 1.1
     */
    void subscribe(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked, Res<PMessage> on_msg,
                   Res<QString> on_done = _nop_res_status, Res<QString> on_handle = _nop_res_status);

    /**
     * @brief Subscribe to a MsgPack resource
     * @param uri The resource to subscribe to
     * @param on_msg The callback to be called for each message
     * @param on_done The callback to be executed with the error message. Empty implies success.
     * @param on_handle The callback to be executed with the subscribe handle.
     *
     * This will unpack msgpack PO's and pass them to the on_msg callback
     *
     * @ingroup qml
     * @since 1.1
     */
    Q_INVOKABLE void subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                                      QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                                      bool doNotVerify, bool leavePacked, Res<QVariantMap> on_msg,
                                      Res<QString> on_done = _nop_res_status,
                                      Res<QString> on_handle = _nop_res_status);

    /**
     * @brief Unsubscribe from a resource
     * @param handle The handle obtained from the on_handle callback parameter to subscribe
     * @param on_done The callback to be executed with the error message. "" implies success.
     */
    void unsubscribe(QString handle, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Unsubscribe from a resource
     * @param handle The handle obtained from the on_handle callback parameter to subscribe
     * @param on_done The callback to be executed with the error message. "" implies success.
     */
    Q_INVOKABLE void unsubscribe(QString handle, QJSValue on_done);

    /**
     * @brief Set the entity
     * @param filename a BW entity file to use
     *
     * @ingroup cpp
     * @since 1.0
     */
    void setEntityFile(QString filename, Res<QString, QString> on_done = _nop_res_status);

    /**
     * @brief Set the entity
     * @param contents the binary contents of an entity descriptor
     *
     * Note that if read from an entity file, the first byte of the file
     * must be stripped as it is an RO type indicator.
     *
     * @see setEntityFile
     * @ingroup cpp
     * @since 1.0
     */
    void setEntity(QByteArray keyfile, Res<QString, QString> on_done = _nop_res_status);

    /**
     * @brief Set the entity by reading the file denoted by $BW2_DEFAULT_ENTITY
     *
     * @see setEntityFile
     * @ingroup cpp
     * @since 1.0
     */
    void setEntityFromEnviron(Res<QString, QString> on_done = _nop_res_status);

    /**
     * @brief buildChain Builds a DOT chain.
     * @param uri The URI to which to build the chain
     * @param permissions The permissions that the chains must provide
     * @param to The entity to which the chain is built
     * @param on_done Called for each chain that is built. Arguments are (1) error message (or empty string if none), (2) DOT chain object (or empty dict if none), and (3) boolean which is true if this is the final DOT chain
     */
    Q_INVOKABLE void buildChain(QString uri, QString permissions, QString to,
                                Res<QString, SimpleChain*, bool> on_done);

    /**
     * @brief buildAnyChain Convenience function that calls buildChain and only gives the first result, if any.
     * @param uri The URI to which to build the chain
     * @param permissions The permissions that the chains must provide
     * @param to The entity to which the chain is built
     * @param on_done Same as BuildChain, but only has the first two arguments.
     */
    Q_INVOKABLE void buildAnyChain(QString uri, QString permissions, QString to,
                                   Res<QString, SimpleChain*> on_done);

    /**
     * @brief Query the given URI pattern and return all messages that match
     * @param uri the URI to query
     * @param on_done A callback receiving the status list of messages
     *
     * @ingroup cpp
     * @since 1.0
     */
    void query(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
               QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
               bool doNotVerify, bool leavePacked,
               Res<QString, PMessage, bool> on_result);

    void queryList(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked,
                   Res<QString, QList<PMessage>> on_done);


    /**
     * @brief list Lists all immediate children of a URI that have persisted messages in their children
     * @param uri The URI whose children to list
     * @param primaryAccessChain The primary access chain
     * @param autoChain
     * @param expiry Expiry time for this message. Ignored if invalid (year == 0)
     * @param expiryDelta Milliseconds after now when this message should expire
     * @param elaboratePAC
     * @param doNotVerify
     * @param on_done
     */
    Q_INVOKABLE void list(QString uri, QString primaryAccessChain, bool autoChain,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, Res<QString, QString, bool> on_done);

    /**
     * @brief publishDOTWithAcc Publish a DOT, using an account to bankroll the operation
     * @param blob The DOT to publish
     * @param account The account number to use
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the hash
     */
    Q_INVOKABLE void publishDOTWithAcc(QByteArray, int account,
                                       Res<QString, QString> on_done);

    /**
     * @brief publishDOT Like publishDOTWithAcc, except that you don't have to explicitly specify the account
     * @param blob
     * @param on_done
     */
    Q_INVOKABLE void publishDOT(QByteArray blob, Res<QString, QString> on_done);


    /**
     * @brief publishEntityWithAcc Publish an entity, using an account to bankroll the operation
     * @param blob The entity to publish
     * @param account The account number to use
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the VK
     */
    Q_INVOKABLE void publishEntityWithAcc(QByteArray blob, int account,
                                          Res<QString, QString> on_done);

    void publishEntity(QByteArray blob, Res<QString, QString> on_done);

    /**
     * @brief setMetadata Sets metadata published at the URI
     * @param uri
     * @param key
     * @param val
     * @param on_done Callback invoked with a single argument: an error message, or the empty string if there was no error
     */
    Q_INVOKABLE void setMetadata(QString uri, QString key, QString val, Res<QString> on_done);

    Q_INVOKABLE void delMetadata(QString uri, QString key, Res<QString> on_done);

    void getMetadata(QString uri, Res<QString, QMap<QString, MetadataTuple>, QMap<QString, QString>> on_tuple);

    void publishChainWithAcc(QByteArray blob, int account,
                             Res<QString, QString> on_done);

    void publishChain(QByteArray blob, Res<QString, QString> on_done);

    void unresolveAlias(QByteArray blob, Res<QString, QString> on_done);

    void resolveLongAlias(QString al, Res<QString, QByteArray, bool> on_done);

    void resolveShortAlias(QString al, Res<QString, QByteArray, bool> on_done);

    void resolveEmbeddedAlias(QString al, Res<QString, QString> on_done);

    void resolveRegistry(QString key, Res<QString, RoutingObject*, RegistryValidity> on_done);

    void entityBalances(Res<QString, QVector<struct balanceinfo>> on_done);

    void addressBalance(QString addr, Res<QString, struct balanceinfo> on_done);

    void getBCInteractionParams(Res<QString, struct currbcip> on_done);

    void setBCInteractionParams(int64_t confirmations, int64_t timeout, int64_t maxAge,
                                Res<QString, struct currbcip> on_done);

    void transferEther(int from, QString to, double ether, Res<QString> on_done);

    void newDesignatedRouterOffer(int account, QString nsvk, Entity* dr, Res<QString> on_done);

    void revokeDesignatedRouterOffer(int account, QString nvsk, Entity* dr, Res<QString> on_done);

    void revokeAcceptanceOfDesignatedRouterOffer(int account, QString drvk, Entity* dr, Res<QString> on_done);

    void revokeEntity(QString vk, Res<QString, QString, QByteArray> on_done);

    void revokeDOT(QString hash, Res<QString, QString, QByteArray> on_done);

    void publishRevocation(int account, QByteArray blob, Res<QString, QString> on_done);

    void getDesignatedRouterOffers(QString nsvk, Res<QString, QString, QString, QList<QString>> on_done);

    void acceptDesignatedRouterOffer(int account, QString drvk, Entity* ns, Res<QString> on_done);

    void setDesignatedRouterSRVRecord(int account, QString srv, Entity* dr, Res<QString> on_done);

    void createLongAlias(int account, QByteArray key, QByteArray val, Res<QString> on_done);

    /**
     * @brief Get the current entity's verifying key
     * @return The base64 version of the VK
     *
     * @ingroup qml
     * @since 1.1.7
     */
    Q_INVOKABLE QString getVK();

    /**
     * @brief Create a new BOSSWAVE View
     * @param query The view expression
     * @param on_done A callback to be executed with an error message (or nil) and the View
     *
     * @see https://github.com/immesys/bw2/wiki/Views
     * @ingroup qml
     * @since 1.2
     */
    Q_INVOKABLE void createView(QVariantMap query, Res<QString, BWView*> on_done);

    /**
     * @brief Create a new BOSSWAVE View
     * @param query The view expression
     * @param on_done A callback to be executed with an error message (or nil) and the View
     *
     * @see https://github.com/immesys/bw2/wiki/Views
     * @ingroup qml
     * @since 1.2
     */
    Q_INVOKABLE void createView(QVariantMap query, QJSValue on_done);

signals:
    /**
     * @brief Fired when the BOSSWAVE agent connection changes (connect or disconnect)
     * @param success True if ok, false otherwise
     * @param msg Empty if ok, error message otherwise
     */
    void agentChanged(bool success, QString msg);

private:
    QQmlEngine *engine;
    QJSEngine *jsengine;
    AgentConnection *agent();
    AgentConnection *m_agent;
    QString m_vk;

    template <typename ...Tz> Res<Tz...> ERes(QJSValue callback)
    {
        return Res<Tz...>(engine, callback);
    }

    static Res<QString> _nop_res_status;
    friend BWView;
};

class BalanceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString addr MEMBER addr)
    Q_PROPERTY(QString human MEMBER human)
    Q_PROPERTY(QString decimal MEMBER decimal)
    Q_PROPERTY(qreal value MEMBER value)

public:

    QString addr;
    QString human;
    QString decimal;
    qreal value;
};

class MetadataTuple
{
public:
    MetadataTuple()
        : value(), timestamp(0) {}

    MetadataTuple(QString val, int64_t ts)
        : value(val), timestamp(ts) {}

    MetadataTuple(const QVariantMap& metadata)
        : value(metadata["val"].toString()), timestamp(metadata["ts"].toLongLong()) {}

    QVariantMap toVariantMap()
    {
        QVariantMap metadata;
        metadata["val"] = this->value;
        metadata["ts"] = QVariant::fromValue(this->timestamp);
        return metadata;
    }

    MetadataTuple& operator=(const MetadataTuple& other)
    {
        this->value = other.value;
        this->timestamp = other.timestamp;
        return *this;
    }

    QString value;
    int64_t timestamp;
};

struct currbcip
{
    int64_t confirmations;
    int64_t timeout;
    int64_t maxAge;
    int64_t currentAge;
    uint64_t currentBlock;
    int64_t peers;
    int64_t highestBlock;
    int64_t difficulty;
};

struct balanceinfo
{
    QString addr;
    QString human;
    QString decimal;
    qreal value;
};

class SimpleChain : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hash MEMBER hash)
    Q_PROPERTY(QString permissions MEMBER permissions)
    Q_PROPERTY(QString uri MEMBER uri)
    Q_PROPERTY(QString to MEMBER to)
    Q_PROPERTY(QString content MEMBER content)
    Q_PROPERTY(bool valid MEMBER valid)

public:
    QString hash;
    QString permissions;
    QString uri;
    QString to;
    QString content;
    bool valid;
};

class BWView : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY interfacesChanged)
    Q_PROPERTY(QStringList services READ services NOTIFY servicesChanged)

public:
    BWView(BW* parent) : QObject(parent), bw(parent) {

    }
    const QStringList& services();
    const QVariantList& interfaces();
signals:
    void interfacesChanged();
    void servicesChanged();
private:
    BW* bw;
    int m_vid;
    QVariantList m_interfaces;
    QStringList m_services;
    void onChange();
    friend BW;
};
//Q_DECLARE_METATYPE(BWView*)


#endif // BOSSWAVE_H


