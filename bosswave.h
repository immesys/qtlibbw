#ifndef QTLIBBW_BOSSWAVE_H
#define QTLIBBW_BOSSWAVE_H

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
    void connectAgent(QByteArray &ourentity);

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
    void createEntity(QDateTime expiry, qreal expiryDelta, QString contact,
                      QString comment, QList<QString> revokers, bool omitCreationDate,
                      Res<QString, QString, QByteArray> on_done);

    /**
     * @brief createEntity Javascript version of createEntity
     * @param params A map of parameters. Keys are: (1) Expiry, (2) ExpiryDelta, (3) Contact, (4) Comment, (5) Revokers, and (6) OmitCreationDate.
     * @param on_done Javascript callback
     */
    Q_INVOKABLE void createEntity(QVariantMap params, QJSValue on_done);

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
    void createDOT(bool isPermission, QString to, unsigned int ttl, QDateTime expiry,
                   qreal expiryDelta, QString contact, QString comment,
                   QList<QString> revokers, bool omitCreationDate, QString uri,
                   QString accessPermissions, QVariantMap appPermissions,
                   Res<QString, QString, QByteArray> on_done);

    /**
     * @brief createDOT Javscript version of createDOT
     * @param params A map of parameters. Keys are: (1) IsPermission, (2) To, (3) TTL, (4) Expiry, (5) ExpiryDelta, (6) Contact, (7) Comment, (8) Revokers, (9) OmitCreationDate, (10) URI, (11) AccessPermissions, and (12) AppPermissions
     * @param on_done Javascript callback
     */
    Q_INVOKABLE void createDOT(QVariantMap params, QJSValue on_done);

    /**
     * @brief createDOTChain Create a Chain of DOTs
     * @param dots The dots to use
     * @param isPermission True if this is a permission chain (not supported)
     * @param unElaborate
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) the hash of the DOT chain, and (3) the DOT chain itself
     */
    void createDOTChain(QList<QString> dots, bool isPermission, bool unElaborate,
                        Res<QString, QString, RoutingObject*> on_done);

    Q_INVOKABLE void createDOTChain(QVariantMap params, QJSValue on_done);

    /**
     * @brief publish Publish to a resource
     * @param uri The resource to publish to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param poz Payload objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param persist If true, the message is persisted
     * @param on_done The callback that is executed when the publish process is complete. Takes one argument: an error message, or the empty string if there was no error
     */
    void publish(QString uri, QString primaryAccessChain, bool autoChain,
                 QList<RoutingObject*> roz, QList<PayloadObject*> poz,
                 QDateTime expiry, qreal expiryDelta, QString elaboratePAC, bool doNotVerify,
                 bool persist, Res<QString> on_done = _nop_res_status);

    /**
     * @brief publish Publish a MagPack object to a resource
     * @param uri The resource to publish to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param ponum The payload object number for the message
     * @param val The message, as a QVariantMap
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param persist If true, the message is persisted
     * @param on_done The callback that is executed when the publish process is complete. Takes one argument: an error message, or the empty string if there was no error
     */
    void publishMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                        int ponum, QVariantMap val, QDateTime expiry, qreal expiryDelta,
                        QString elaboratePAC, bool doNotVerify, bool persist,
                        Res<QString> on_done = _nop_res_status);

    Q_INVOKABLE void publishMsgPack(QVariantMap params, QJSValue on_done);

    /**
     * @brief publish Publish text to a resource
     * @param uri The resource to publish to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param ponum The payload object number for the message
     * @param val The message, as a QVariantMap
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param persist If true, the message is persisted
     * @param on_done The callback that is executed when the publish process is complete. Takes one argument: an error message, or the empty string if there was no error
     */
    void publishText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                     int ponum, QString msg, QDateTime expiry, qreal expiryDelta,
                     QString elaboratePAC, bool doNotVerify, bool persist,
                     Res<QString> on_done = _nop_res_status);

    Q_INVOKABLE void publishText(QVariantMap params, QJSValue on_done);

    /**
     * @brief subscribe Subscribe to a resource
     * @param uri The resource to subscribe to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_msg The callback that is executed when a message is received
     * @param on_done The callback that is executed when the subscribe process is complete. First argument is an error message, or the empty string if no error occurred. Second argument is the subscription handle.
     */
    void subscribe(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked, Res<PMessage> on_msg,
                   Res<QString, QString> on_done = _nop_res_status);

    /**
     * @brief subscribeText Subscribe to a text resource
     * @param uri The resource to subscribe to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_msg The callback that is executed when a message is received, with two arguments: (1) the PO number, and (2) the unpacked contents
     * @param on_done The callback that is executed when the subscribe process is complete
     */
    void subscribeMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                          QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                          bool doNotVerify, bool leavePacked, Res<int, QVariantMap> on_msg,
                          Res<QString, QString> on_done = _nop_res_status);

    Q_INVOKABLE void subscribeMsgPack(QVariantMap params, QJSValue on_msg, QJSValue on_done);

    /**
     * @brief subscribeText Subscribe to a text resource
     * @param uri The resource to subscribe to
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_msg The callback that is executed when a message is received, with two arguments: (1) the PO number, and (2) the text contents
     * @param on_done The callback that is executed when the subscribe process is complete. It takes one argument: an error message, or the empty string if there was no error
     */
    void subscribeText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                       QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                       bool doNotVerify, bool leavePacked, Res<int, QString> on_msg,
                       Res<QString, QString> on_done = _nop_res_status);

    Q_INVOKABLE void subscribeText(QVariantMap params, QJSValue on_msg, QJSValue on_done);

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
    void setEntityFile(QString filename, Res<QString, QString> on_done);

    Q_INVOKABLE void setEntityFile(QString filename, QJSValue on_done);

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
    void setEntity(QByteArray keyfile, Res<QString, QString> on_done);

    Q_INVOKABLE void setEntity(QByteArray keyfile, QJSValue on_done);

    /**
     * @brief Set the entity by reading the file denoted by $BW2_DEFAULT_ENTITY
     *
     * @see setEntityFile
     * @ingroup cpp
     * @since 1.0
     */
    void setEntityFromEnviron(Res<QString, QString> on_done);

    Q_INVOKABLE void setEntityFromEnviron(QJSValue on_done);

    /**
     * @brief buildChain Builds a DOT chain.
     * @param uri The URI to which to build the chain
     * @param permissions The permissions that the chains must provide
     * @param to The entity to which the chain is built
     * @param on_done Called for each chain that is built. Arguments are (1) error message (or empty string if none), (2) DOT chain object (or empty dict if none), and (3) boolean which is true if this is the final DOT chain
     */
    void buildChain(QString uri, QString permissions, QString to,
                    Res<QString, SimpleChain*, bool> on_done);

    Q_INVOKABLE void buildChain(QVariantMap params, QJSValue on_done);

    /**
     * @brief buildAnyChain Convenience function that calls buildChain and only gives the first result, if any.
     * @param uri The URI to which to build the chain
     * @param permissions The permissions that the chains must provide
     * @param to The entity to which the chain is built
     * @param on_done Same as BuildChain, but only has the first two arguments.
     */
    void buildAnyChain(QString uri, QString permissions, QString to,
                       Res<QString, SimpleChain*> on_done);

    Q_INVOKABLE void buildAnyChain(QVariantMap params, QJSValue on_done);

    /**
     * @brief query Query a resource for persisted messages
     * @param uri The resource to query
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_result Callback that is invoked multiple times. Takes three arguments: (1) error message, or the empty string if there was no error, (2) a persisted message or nullptr, (3) a boolean indicating whether all persisted messages have been delivered (in which case the callback will not be invoked again)
     */
    void query(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
               QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
               bool doNotVerify, bool leavePacked,
               Res<QString, PMessage, bool> on_result);

    /**
     * @brief queryMsgPack Query a resource for persisted MsgPack messages and decode them as MsgPack
     * @param uri The resource to query
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_result Callback that is invoked multiple times. Takes five arguments: (1) error message, or the empty string if there was no error, (2) the payload object number of a persisted message, (3) the decoded message, as a QVariantMap, (4) a boolean indicating whether a message is included in this invocation, and (5) a boolean indicating whether all persisted messages have been delivered (in which case the callback will not be invoked again)
     */
    void queryMsgPack(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                      QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                      bool doNotVerify, bool leavePacked,
                      Res<QString, int, QVariantMap, bool, bool> on_result);

    Q_INVOKABLE void queryMsgPack(QVariantMap params, QJSValue on_result);

    /**
     * @brief queryText Query a resource for persisted text messages and decode them as text
     * @param uri The resource to query
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_result Callback that is invoked multiple times. Takes five arguments: (1) error message, or the empty string if there was no error, (2) the payload object number of a persisted message, (3) the decoded message, as a QString, (4) a boolean indicating whether a message is included in this invocation, and (5) a boolean indicating whether all persisted messages have been delivered (in which case the callback will not be invoked again)
     */
    void queryText(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked,
                   Res<QString, int, QString, bool, bool> on_result);

    Q_INVOKABLE void queryText(QVariantMap params, QJSValue on_result);

    /**
     * @brief queryList Query a resource for persisted messages, returning all of them as a list
     * @param uri The resource to query
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_result Callback that is invoked once. Takes two arguments: (1) error message, or the empty string if there was no error, (2) a list of persisted messages
     */
    void queryList(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                   QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                   bool doNotVerify, bool leavePacked,
                   Res<QString, QList<PMessage>> on_done);

    /**
     * @brief queryList Query a resource for persisted messages, giving only one result
     * @param uri The resource to query
     * @param primaryAccessChain The Primary Access Chain to use
     * @param autoChain If true, the DOT chain is inferred automatically
     * @param roz Routing objects to include in the message
     * @param expiry The time at which the message should expire (ignored if invalid)
     * @param expiryDelta The number of milliseconds after which the message should expire (ignored if negative)
     * @param elaboratePAC Elaboration level for the Primary Access Chain
     * @param doNotVerify If false, the router will verify this message as if it were hostile
     * @param leavePacked If true, the POs and ROs are left in the bosswave format
     * @param on_result Callback that is invoked once. Takes two arguments: (1) error message, or the empty string if there was no error, (2) a persisted message, or nullptr if none are present
     */
    void queryOne(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
                  QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
                  bool doNotVerify, bool leavePacked, Res<QString, PMessage> on_done);

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
    void list(QString uri, QString primaryAccessChain, bool autoChain, QList<RoutingObject*> roz,
              QDateTime expiry, qreal expiryDelta, QString elaboratePAC,
              bool doNotVerify, Res<QString, QString, bool> on_result);

    Q_INVOKABLE void list(QVariantMap params, QJSValue on_result);

    /**
     * @brief publishDOTWithAcc Publish a DOT, using an account to bankroll the operation
     * @param blob The DOT to publish
     * @param account The account number to use
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the hash
     */
    void publishDOTWithAcc(QByteArray blob, int account, Res<QString, QString> on_done);

    Q_INVOKABLE void publishDOTWithAcc(QByteArray blob, int account, QJSValue on_done);

    /**
     * @brief publishDOT Like publishDOTWithAcc, but always uses account number 1
     * @param blob The DOT to publish
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the hash
     */
    void publishDOT(QByteArray blob, Res<QString, QString> on_done);

    Q_INVOKABLE void publishDOT(QByteArray blob, QJSValue on_done);


    /**
     * @brief publishEntityWithAcc Publish an entity, using an account to bankroll the operation
     * @param blob The entity to publish
     * @param account The account number to use
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the VK
     */
    void publishEntityWithAcc(QByteArray blob, int account, Res<QString, QString> on_done);

    Q_INVOKABLE void publishEntityWithAcc(QByteArray blob, int account, QJSValue on_done);

    /**
     * @brief publishEntity Like publishEntityWithAcc, but always uses account number 1
     * @param blob The entity to publish
     * @param on_done Callback called with two arguments: (1) the error message (or empty string if no error), and (2) the VK
     */
    void publishEntity(QByteArray blob, Res<QString, QString> on_done);

    Q_INVOKABLE void publishEntity(QByteArray blob, QJSValue on_done);

    /**
     * @brief setMetadata Sets metadata published at the URI
     * @param uri The URI at which the metadata should be set
     * @param key The metadata key to set
     * @param val The value to which the key should be bound
     * @param on_done Callback invoked with a single argument: an error message, or the empty string if there was no error
     */
    void setMetadata(QString uri, QString key, QString val, Res<QString> on_done);

    Q_INVOKABLE void setMetadata(QString uri, QString key, QString val, QJSValue on_done);

    /**
     * @brief delMetadata Deletes the metadata, for a given key, published at a URI
     * @param uri The URI from which the key should be deleted
     * @param key The metadata key to delete
     * @param on_done Callback invoked with a single argument: an error message, or the empty string if there was no error
     */
    void delMetadata(QString uri, QString key, Res<QString> on_done);

    Q_INVOKABLE void delMetadata(QString uri, QString key, QJSValue on_done);

    /**
     * @brief getMetadata Get all of the metadata at a URI
     * @param uri The URI at which to resolve the metadata
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) a map with the key-value pairs of that metadata, and (3) a map indicating the URI at which each key was resolved
     */
    void getMetadata(QString uri, Res<QString, QMap<QString, MetadataTuple>, QMap<QString, QString>> on_done);

    Q_INVOKABLE void getMetadata(QString uri, QJSValue on_done);

    /**
     * @brief getMetadataKey Get the metadata at a URI corresponding to a single key
     * @param uri The URI at which to resolve the metadata
     * @param key The key whose value is to be resolved
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) the value corresponding to that key, and (3) the URI at which the key was resolved
     */
    void getMetadataKey(QString uri, QString key, Res<QString, MetadataTuple, QString> on_done);

    Q_INVOKABLE void getMetadataKey(QString uri, QString key, QJSValue on_done);

    /**
     * @brief publishChainWithAcc Publish a DOT chain using the specified account number
     * @param blob The DOT chain as a byte array
     * @param account The acccount number to use
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the hash of the DOT chain
     */
    void publishChainWithAcc(QByteArray blob, int account, Res<QString, QString> on_done);

    Q_INVOKABLE void publishChainWithAcc(QByteArray blob, int account, QJSValue on_done);

    /**
     * @brief publishChain Like publishChainWithAcc, but uses account 1
     * @param blob
     * @param on_done
     */
    void publishChain(QByteArray blob, Res<QString, QString> on_done);

    Q_INVOKABLE void publishChain(QByteArray blob, QJSValue on_done);

    /**
     * @brief unresolveAlias Unresolves an entity to an alias
     * @param blob The entity to unresolve, as a byte array
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the alias that the entity unresolved to
     */
    void unresolveAlias(QByteArray blob, Res<QString, QString> on_done);

    Q_INVOKABLE void unresolveAlias(QByteArray blob, QJSValue on_done);

    /**
     * @brief resolveLongAlias Resolve a long alias to a byte array
     * @param al The long alias to resolve
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) the byte array that the long alias resolved to, and (3) a boolean indicating whether the long alias was successfully resolved
     */
    void resolveLongAlias(QString al, Res<QString, QByteArray, bool> on_done);

    Q_INVOKABLE void resolveLongAlias(QString al, QJSValue on_done);

    /**
     * @brief resolveShortAlias Resolve a short alias to a byte array
     * @param al The short alias to resolve
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) the byte array that the short alias resolved to, and (3) a boolean indicating whether the short alias was successfully resolved
     */
    void resolveShortAlias(QString al, Res<QString, QByteArray, bool> on_done);

    Q_INVOKABLE void resolveShortAlias(QString al, QJSValue on_done);

    /**
     * @brief resolveEmbeddedAlias Resolve an embedded alias to a string
     * @param al The embedded alias to resolve
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the data that the embedded alias resolved to
     */
    void resolveEmbeddedAlias(QString al, Res<QString, QString> on_done);

    Q_INVOKABLE void resolveEmbeddedAlias(QString al, QJSValue on_done);

    /**
     * @brief resolveRegistry Resolve a key to a Routing Object in the registry
     * @param key The key to resolve
     * @param on_done Callback invoked with three arguments: (1) an error message, or the empty string if there was no error, (2) the Routing Object that the key resolved to, and (3) an enumeration indicating the result of the resolution
     */
    void resolveRegistry(QString key, Res<QString, RoutingObject*, RegistryValidity> on_done);

    Q_INVOKABLE void resolveRegistry(QString key, QJSValue on_done);

    /**
     * @brief entityBalances Get the balances of the current entity's bank accounts
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the balances of this entity's bank accounts
     */
    void entityBalances(Res<QString, QVector<struct balanceinfo>> on_done);

    Q_INVOKABLE void entityBalances(QJSValue on_done);

    /**
     * @brief addressBalance Get the balance of the bank account with the given address
     * @param addr The address of the bank account whose balance to query
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the balance of the specified bank account
     */
    void addressBalance(QString addr, Res<QString, struct balanceinfo> on_done);

    Q_INVOKABLE void addressBalance(QString addr, QJSValue on_done);

    /**
     * @brief getBCInteractionParams Get the Block Chain Interaction Parameters
     * @param on_done Callback invoked with two arguments: (1) an error message, or the empty string if there was no error, and (2) the interaction parameters
     */
    void getBCInteractionParams(Res<QString, struct currbcip> on_done);

    /**
     * @brief setBCInteractionParams Sets the Block Chain Interaction Parameters
     * @param confirmations The new number of confirmations (ignored if negative)
     * @param timeout The new timeout (ignored if negative)
     * @param maxAge The new max age (ignored if negative)
     * @param on_done Callback (same as getBCInteractionParams)
     */
    void setBCInteractionParams(int64_t confirmations, int64_t timeout, int64_t maxAge,
                                Res<QString, struct currbcip> on_done);

    /**
     * @brief transferEther Transfer Ether from this entity to another entity
     * @param from The bank account of this entity from which to tranfer Ether
     * @param to The entity to which Ether should be transferred
     * @param ether The number of Ether to transfer
     * @param on_done Callback called with a single argument: an error message, or the empty string if there was no error
     */
    void transferEther(int from, QString to, double ether, Res<QString> on_done);

    /**
     * @brief newDesignatedRouterOffer Make a new Designated Router Offer
     * @param account The account to use to make the offer
     * @param nsvk The namespace verifying key
     * @param dr The router to which to make the offer
     * @param on_done Callback called with a single argument: an error message, or the empty string if there was no error
     */
    void newDesignatedRouterOffer(int account, QString nsvk, Entity* dr, Res<QString> on_done);

    /**
     * @brief revokeDesignatedRouterOffer Revoke a Designated Router Offer
     * @param account The account to use to revoke the offer
     * @param nvsk The namespace verifying key
     * @param dr The router to which the offer was made
     * @param on_done Callback called with a single argument: an error message, or the empty string if there was no error
     */
    void revokeDesignatedRouterOffer(int account, QString nvsk, Entity* dr, Res<QString> on_done);

    /**
     * @brief revokeAcceptanceOfDesignatedRouterOffer Revoke acceptance of a Designated Router Offer
     * @param account The account to use to revoke the offer
     * @param drvk The verifying key of the designated router
     * @param ns The namespace
     * @param on_done Callback called with a single argument: an error message, or the empty string if there was no error
     */
    void revokeAcceptanceOfDesignatedRouterOffer(int account, QString drvk, Entity* ns, Res<QString> on_done);

    /**
     * @brief revokeEntity Revoke the entity with the specified VK
     * @param vk The verifying key
     * @param on_done Callback called with three arguments: (1) an error message, or the empty string if there was no error, (2) the hash, and (3) the contents of the Payload Object
     */
    void revokeEntity(QString vk, Res<QString, QString, QByteArray> on_done);

    /**
     * @brief revokeDOT Revoke the DOT with the specified hash
     * @param hash The hash of the dot to revoke
     * @param on_done Callback called with three arguments: (1) an error message, or the empty string if there was no error, (2) the hash, and (3) the contents of the Payload Object
     */
    void revokeDOT(QString hash, Res<QString, QString, QByteArray> on_done);

    /**
     * @brief publishRevocation Publishes a revocation
     * @param account The account to use
     * @param blob The revocation to publish
     * @param on_done Callback called with two arguments: (1) an error message, or the empty string if there was no error, and (2) the hash
     */
    void publishRevocation(int account, QByteArray blob, Res<QString, QString> on_done);

    /**
     * @brief getDesignatedRouterOffers Get the designated router offers
     * @param nsvk The namespace verifying key
     * @param on_done Callback invoked with four arguments: (1) an error message, or the empty string if there was no error, (2) the contents of the active header, (3) the contents of the srv header, and (4) the offers, as a list of designated router verifying keys
     */
    void getDesignatedRouterOffers(QString nsvk, Res<QString, QString, QString, QList<QString>> on_done);

    /**
     * @brief acceptDesignatedRouterOffer Accept a designated router offer
     * @param account The account number to use
     * @param drvk The designated router verifying key to use
     * @param ns The namespace
     * @param on_done Callback invoked with one argument: an error message, or the empty string if there was no error
     */
    void acceptDesignatedRouterOffer(int account, QString drvk, Entity* ns, Res<QString> on_done);

    /**
     * @brief setDesignatedRouterSRVRecord Sets the designated router SRV record
     * @param account The account number to use
     * @param srv The SRV record
     * @param dr The designated router
     * @param on_done Callback invoked with one argument: an error message, or the empty string if there was no error
     */
    void setDesignatedRouterSRVRecord(int account, QString srv, Entity* dr, Res<QString> on_done);

    /**
     * @brief createLongAlias Creates a long alias
     * @param account The account number to use
     * @param key The new alias (at most 32 bytes)
     * @param val The value that the alias resolves to (at most 32 bytes)
     * @param on_done Callback invoked with one argument: an error message, or the empty string if there was no error
     */
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

class MetadataTupleJS : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString value MEMBER value)
    Q_PROPERTY(QDateTime time MEMBER time)

public:
    MetadataTupleJS(QObject* parent = nullptr) : QObject(parent) {}

    MetadataTupleJS(QString val, int64_t ts, QObject* parent = nullptr)
        : QObject(parent), value(val)
    {
        int64_t msecs = ts / 1000000;
        int64_t nsecs = ts % 1000000;
        if (nsecs < 0)
        {
            msecs -= 1;
            nsecs += 1000000;
        }

        /* Round to the nearest millisecond. */
        if (nsecs >= 500000)
        {
            msecs += 1;
        }
        time = QDateTime::fromMSecsSinceEpoch(msecs);
    }

    QString value;
    QDateTime time;
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

class BalanceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString addr MEMBER addr)
    Q_PROPERTY(QString human MEMBER human)
    Q_PROPERTY(QString decimal MEMBER decimal)
    Q_PROPERTY(qreal value MEMBER value)

public:
    BalanceInfo(QObject* parent = nullptr) : QObject(parent), value(0.0) {}
    BalanceInfo(const struct balanceinfo& bi, QObject* parent = nullptr)
        : QObject(parent), addr(bi.addr), human(bi.human), decimal(bi.decimal),
          value(bi.value) {}

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


#endif // QTLIBBW_BOSSWAVE_H


