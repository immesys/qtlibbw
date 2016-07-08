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

QT_FORWARD_DECLARE_CLASS(SimpleChain)
QT_FORWARD_DECLARE_CLASS(BWView)


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
                                  Res<QString, QString, QString> on_done);

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
                               Res<QString, QString, QString> on_done);

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
     * @brief Set the entity
     * @param filename a BW entity file to use
     *
     * @ingroup cpp
     * @since 1.0
     */
    void setEntityFile(QString filename, Res<QString> on_done = _nop_res_status);

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
    void setEntity(QByteArray &contents, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Set the entity by reading the file denoted by $BW2_DEFAULT_ENTITY
     *
     * @see setEntityFile
     * @ingroup cpp
     * @since 1.0
     */
    void setEntityFromEnviron(Res<QString> on_done = _nop_res_status);

    /**
     * @brief publish a bosswave message
     * @param uri the URI to publish to
     * @param poz the payload objects to publish
     * @param on_done a function to call when the operation is complete.
     *
     * @ingroup cpp
     * @since 1.0
     */
    void publish(QString uri, QList<PayloadObject*> poz, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Publish a MsgPack object to the given URI
     *
     * @ingroup qml
     * @since 1.0
     */
    Q_INVOKABLE void publishMsgPack(QString uri, QString PODF, QVariantMap msg, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Publish a MsgPack object to the given URI
     *
     * @ingroup qml
     * @since 1.0
     */
    Q_INVOKABLE void publishMsgPack(QString uri, int PONum, QVariantMap msg, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Publish a MsgPack object to the given URI, with a javascript callback
     *
     * @ingroup qml
     * @since 1.0
     */
    Q_INVOKABLE void publishMsgPack(QString uri, QString PODF, QVariantMap msg, QJSValue on_done);

    /**
     * @brief Publish a MsgPack object to the given URI, with a javascript callback
     *
     * @ingroup qml
     * @since 1.0
     */
    Q_INVOKABLE void publishMsgPack(QString uri, int PONum, QVariantMap msg, QJSValue on_done);

    /**
     * @brief Publish plain text to the given URI
     *
     * @ingroup qml
     * @since 1.3
     */
    Q_INVOKABLE void publishText(QString uri, QString msg, Res<QString> on_done = _nop_res_status);

    /**
     * @brief Publish plain text to the given URI, with a javascript callback
     *
     * @ingroup qml
     * @since 1.3
     */
    Q_INVOKABLE void publishText(QString uri, QString msg, QJSValue on_done);

    /**
      * @brief Publish text of the specified type to the given URI
      *
      * @ingroup qml
      * @since 1.3
      */
    Q_INVOKABLE void publishText(QString uri, int PONum, QString msg, Res<QString> on_done = _nop_res_status);

    /**
      * @brief Publish text of the specified type to the given URI, with a javascript callback
      *
      * @ingroup qml
      * @since 1.3
      */
    Q_INVOKABLE void publishText(QString uri, int PONum, QString msg, QJSValue on_done);

    /**
     * @brief Query the given URI pattern and return all messages that match
     * @param uri the URI to query
     * @param on_done A callback receiving the status list of messages
     *
     * @ingroup cpp
     * @since 1.0
     */
    void query(QString uri, Res<QString, QList<PMessage>> on_done);

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
    void subscribe(QString uri, Res<PMessage> on_msg, Res<QString> on_done = _nop_res_status,
                   Res<QString> on_handle = _nop_res_status);

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
    Q_INVOKABLE void subscribeMsgPack(QString uri, Res<QVariantMap> on_msg, Res<QString> on_done = _nop_res_status,
                                      Res<QString> on_handle = _nop_res_status);

    /**
     * @brief Subscribe to a MsgPack resource
     * @param uri The resource to subscribe to
     * @param on_msg The callback to be called for each message (javascript function)
     *
     * @ingroup qml
     * @since 1.1
     */
    Q_INVOKABLE void subscribeMsgPack(QString uri, QJSValue on_msg);

    /**
     * @brief Subscribe to a MsgPack resource
     * @param uri The resource to subscribe to
     * @param on_msg The callback to be called for each message (javascript function)
     * @param on_done The callback to be executed with the error message. "" implies success.
     *
     * @ingroup qml
     * @since 1.1
     */
    Q_INVOKABLE void subscribeMsgPack(QString uri, QJSValue on_msg, QJSValue on_done);

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

class SimpleChain : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hash MEMBER hash)
    Q_PROPERTY(QString permissions MEMBER permissions)
    Q_PROPERTY(QString uri MEMBER uri)
    Q_PROPERTY(QString to MEMBER to)
    Q_PROPERTY(QString content MEMBER content)
    Q_PROPERTY(bool valid MEMBER valid);

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


