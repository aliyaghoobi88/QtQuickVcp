/****************************************************************************
**
** This file was generated by a code generator based on imatix/gsl
** Any changes in this file will be lost.
**
****************************************************************************/
#include "publish.h"
#include <google/protobuf/text_format.h>
#include "debughelper.h"

#if defined(Q_OS_IOS)
namespace gpb = google_public::protobuf;
#else
namespace gpb = google::protobuf;
#endif

using namespace nzmqt;

namespace machinetalk { namespace common {

/** Generic Publish implementation */
Publish::Publish(QObject *parent)
    : QObject(parent)
    , m_ready(false)
    , m_debugName(QStringLiteral("Publish"))
    , m_socketUri(QStringLiteral(""))
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_state(State::Down)
    , m_previousState(State::Down)
    , m_errorString(QStringLiteral(""))
    , m_heartbeatInterval(2500)
{

    m_heartbeatTimer.setSingleShot(true);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &Publish::heartbeatTimerTick);
    // state machine
    connect(this, &Publish::fsmDownStart,
            this, &Publish::fsmDownStartEvent);
    connect(this, &Publish::fsmUpStop,
            this, &Publish::fsmUpStopEvent);
    connect(this, &Publish::fsmUpHeartbeatTick,
            this, &Publish::fsmUpHeartbeatTickEvent);

    m_context = new PollingZMQContext(this, 1);
    connect(m_context, &PollingZMQContext::pollError,
            this, &Publish::socketError);
    m_context->start();
}

Publish::~Publish()
{
    stopSocket();

    if (m_context != nullptr)
    {
        m_context->stop();
        m_context->deleteLater();
        m_context = nullptr;
    }
}

/** Add a topic that should be subscribed **/
void Publish::addSocketTopic(const QByteArray &name)
{
    m_socketTopics.insert(name);
}

/** Removes a topic from the list of topics that should be subscribed **/
void Publish::removeSocketTopic(const QByteArray &name)
{
    m_socketTopics.remove(name);
}

/** Clears the the topics that should be subscribed **/
void Publish::clearSocketTopics()
{
    m_socketTopics.clear();
}

/** Connects the 0MQ sockets */
bool Publish::startSocket()
{
    m_socket = m_context->createSocket(ZMQSocket::TYP_XPUB, this);
    m_socket->setLinger(0);

    try {
        m_socket->bindTo(m_socketUri);
    }
    catch (const zmq::error_t &e) {
        const QString errorString = QStringLiteral("Error %1: ").arg(e.num()) + QString(e.what());
        qCritical() << m_debugName << ":" << errorString;
        return false;
    }

    connect(m_socket, &ZMQSocket::messageReceived,
            this, &Publish::processSocketMessage);


#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "sockets connected" << m_socketUri);
#endif

    return true;
}

/** Disconnects the 0MQ sockets */
void Publish::stopSocket()
{
    if (m_socket != nullptr)
    {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}


void Publish::resetHeartbeatTimer()
{
    if (m_heartbeatTimer.isActive())
    {
        m_heartbeatTimer.stop();
    }

    if (m_heartbeatInterval > 0)
    {
        m_heartbeatTimer.setInterval(m_heartbeatInterval);
        m_heartbeatTimer.start();
    }
}

void Publish::startHeartbeatTimer()
{
    resetHeartbeatTimer();
}

void Publish::stopHeartbeatTimer()
{
    m_heartbeatTimer.stop();
}

void Publish::heartbeatTimerTick()
{
    if (m_state == State::Up)
    {
        emit fsmUpHeartbeatTick(QPrivateSignal());
    }
}

/** Processes all message received on socket */
void Publish::processSocketMessage(const QList<QByteArray> &messageList)
{
    Container &rx = m_socketRx;

    emit socketMessageReceived(rx);
}

void Publish::sendSocketMessage(const QByteArray &topic, ContainerType type, Container &tx)
{
    if (m_socket == nullptr) {  // disallow sending messages when not connected
        return;
    }

    tx.set_type(type);
#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(tx, &s);
    DEBUG_TAG(3, m_debugName, "sent message" << QString::fromStdString(s));
#endif
    try {
        QList<QByteArray> message;
        message.append(topic);
        message.append(QByteArray(tx.SerializeAsString().c_str(), tx.ByteSize()));
        m_socket->sendMessage(message);
    }
    catch (const zmq::error_t &e) {
        const QString errorString = QStringLiteral("Error %1: ").arg(e.num()) + QString(e.what());
        qCritical() << errorString;
        return;
    }
    tx.Clear();
}

void Publish::sendPing()
{
    Container &tx = m_socketTx;
    for (const auto &topic: qAsConst(m_socketTopics)) {
        sendSocketMessage(topic, MT_PING, tx);
    }
}

void Publish::sendFullUpdate(const QByteArray &topic, Container &tx)
{
    sendSocketMessage(topic, MT_FULL_UPDATE, tx);
}

void Publish::sendIncrementalUpdate(const QByteArray &topic, Container &tx)
{
    sendSocketMessage(topic, MT_INCREMENTAL_UPDATE, tx);
}

void Publish::socketError(int errorNum, const QString &errorMsg)
{
    const QString errorString = QStringLiteral("Error %1: ").arg(errorNum) + errorMsg;
    qCritical() << errorString;
}

void Publish::fsmDown()
{
#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "State DOWN");
#endif
    m_state = State::Down;
    emit stateChanged(m_state);
}

void Publish::fsmDownStartEvent()
{
    if (m_state == State::Down)
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_debugName, "Event START");
#endif
        // handle state change
        emit fsmDownExited(QPrivateSignal());
        fsmUp();
        emit fsmUpEntered(QPrivateSignal());
        // execute actions
        startSocket();
        startHeartbeatTimer();
     }
}

void Publish::fsmUp()
{
#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "State UP");
#endif
    m_state = State::Up;
    emit stateChanged(m_state);
}

void Publish::fsmUpStopEvent()
{
    if (m_state == State::Up)
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_debugName, "Event STOP");
#endif
        // handle state change
        emit fsmUpExited(QPrivateSignal());
        fsmDown();
        emit fsmDownEntered(QPrivateSignal());
        // execute actions
        stopHeartbeatTimer();
        stopSocket();
     }
}

void Publish::fsmUpHeartbeatTickEvent()
{
    if (m_state == State::Up)
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_debugName, "Event HEARTBEAT TICK");
#endif
        // execute actions
        sendPing();
        resetHeartbeatTimer();
     }
}

/** start trigger function */
void Publish::start()
{
    if (m_state == State::Down) {
        emit fsmDownStart(QPrivateSignal());
    }
}

/** stop trigger function */
void Publish::stop()
{
    if (m_state == State::Up) {
        emit fsmUpStop(QPrivateSignal());
    }
}

} } // namespace machinetalk::common
