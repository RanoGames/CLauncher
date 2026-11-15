#include "mc/DiscordRpc.h"
#include "core/Constants.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QUuid>

DiscordRpc &DiscordRpc::instance() {
    static DiscordRpc s_instance;
    return s_instance;
}

DiscordRpc::DiscordRpc(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_clientId(Constants::DISCORD_CLIENT_ID)
    , m_retryTimer(new QTimer(this))
{
    connect(m_socket, &QLocalSocket::connected, this, &DiscordRpc::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DiscordRpc::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DiscordRpc::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DiscordRpc::onErrorOccurred);

    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &DiscordRpc::retryConnection);
}

DiscordRpc::~DiscordRpc() {
    stop();
}

bool DiscordRpc::isConnected() const {
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState && m_handshakeCompleted;
}

QString DiscordRpc::getPipePath(int index) const {
#if defined(Q_OS_WIN)
    return QStringLiteral("\\\\.\\pipe\\discord-ipc-%1").arg(index);
#else
    const QString xdg = QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_RUNTIME_DIR"));
    if (!xdg.isEmpty()) {
        const QString p = QStringLiteral("%1/discord-ipc-%2").arg(xdg).arg(index);
        if (QFileInfo::exists(p)) {
            return p;
        }
    }

    const QString tmp = QProcessEnvironment::systemEnvironment().value(QStringLiteral("TMPDIR"), QStringLiteral("/tmp"));
    const QString pTmp = QStringLiteral("%1/discord-ipc-%2").arg(tmp).arg(index);
    if (QFileInfo::exists(pTmp)) {
        return pTmp;
    }

    // Default fallback to XDG path even if not pre-checked
    if (!xdg.isEmpty()) {
        return QStringLiteral("%1/discord-ipc-%2").arg(xdg).arg(index);
    }
    return pTmp;
#endif
}

void DiscordRpc::start(const QString &version, const QString &details) {
    LOG_INFO(QStringLiteral("Starting Discord RPC for version %1").arg(version));
    m_version = version;
    m_details = details;
    m_startTime = QDateTime::currentSecsSinceEpoch();
    m_active = true;
    m_handshakeSent = false;
    m_handshakeCompleted = false;
    m_currentPipeIndex = 0;

    if (m_socket->state() == QLocalSocket::ConnectedState) {
        sendActivity();
    } else {
        tryNextPipe();
    }
}

void DiscordRpc::updatePresence(const QString &version, const QString &details) {
    m_version = version;
    m_details = details;
    if (isConnected()) {
        sendActivity();
    }
}

void DiscordRpc::stop() {
    if (!m_active && m_socket->state() == QLocalSocket::UnconnectedState) {
        return;
    }

    LOG_INFO(QStringLiteral("Stopping Discord RPC"));
    m_active = false;
    m_retryTimer->stop();

    if (m_socket->state() == QLocalSocket::ConnectedState) {
        // Send close opcode 2 or clear activity
        QJsonObject clearActivity;
        clearActivity[QStringLiteral("cmd")] = QStringLiteral("SET_ACTIVITY");
        QJsonObject args;
        args[QStringLiteral("pid")] = static_cast<qint64>(QCoreApplication::applicationPid());
        args[QStringLiteral("activity")] = QJsonValue::Null;
        clearActivity[QStringLiteral("args")] = args;
        clearActivity[QStringLiteral("nonce")] = QUuid::createUuid().toString(QUuid::WithoutBraces);

        const QByteArray payload = QJsonDocument(clearActivity).toJson(QJsonDocument::Compact);
        sendPacket(1, payload);

        m_socket->disconnectFromServer();
    } else if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_handshakeSent = false;
    m_handshakeCompleted = false;
}

void DiscordRpc::tryNextPipe() {
    if (!m_active) return;

    if (m_currentPipeIndex > 9) {
        // All pipe slots exhausted, retry in 10 seconds
        LOG_DEBUG(QStringLiteral("Discord IPC pipe not found on slots 0-9. Retrying in 10s..."));
        m_currentPipeIndex = 0;
        m_retryTimer->start(10000);
        return;
    }

    const QString path = getPipePath(m_currentPipeIndex);
    LOG_DEBUG(QStringLiteral("Attempting Discord IPC connection on %1").arg(path));

    m_socket->abort();
    m_socket->connectToServer(path);
}

void DiscordRpc::retryConnection() {
    if (!m_active) return;
    if (m_socket->state() == QLocalSocket::ConnectedState) return;

    m_currentPipeIndex = 0;
    tryNextPipe();
}

void DiscordRpc::onConnected() {
    LOG_INFO(QStringLiteral("Connected to Discord IPC on slot %1").arg(m_currentPipeIndex));
    sendHandshake();
    emit connected();
}

void DiscordRpc::onDisconnected() {
    LOG_INFO(QStringLiteral("Disconnected from Discord IPC"));
    m_handshakeSent = false;
    m_handshakeCompleted = false;
    emit disconnected();

    if (m_active) {
        m_retryTimer->start(10000);
    }
}

void DiscordRpc::onErrorOccurred(QLocalSocket::LocalSocketError socketError) {
    Q_UNUSED(socketError);
    if (!m_active) return;

    if (!m_handshakeCompleted && m_currentPipeIndex <= 9) {
        m_currentPipeIndex++;
        tryNextPipe();
    } else {
        m_retryTimer->start(10000);
    }
}

void DiscordRpc::sendPacket(quint32 opcode, const QByteArray &jsonData) {
    if (m_socket->state() != QLocalSocket::ConnectedState) return;

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << opcode;
    stream << static_cast<quint32>(jsonData.size());
    packet.append(jsonData);

    m_socket->write(packet);
    m_socket->flush();
}

void DiscordRpc::sendHandshake() {
    if (m_handshakeSent) return;

    QJsonObject handshake;
    handshake[QStringLiteral("v")] = 1;
    handshake[QStringLiteral("client_id")] = m_clientId;

    const QByteArray data = QJsonDocument(handshake).toJson(QJsonDocument::Compact);
    sendPacket(0, data);
    m_handshakeSent = true;
}

void DiscordRpc::sendActivity() {
    if (m_socket->state() != QLocalSocket::ConnectedState) return;

    QJsonObject root;
    root[QStringLiteral("cmd")] = QStringLiteral("SET_ACTIVITY");

    QJsonObject args;
    args[QStringLiteral("pid")] = static_cast<qint64>(QCoreApplication::applicationPid());

    QJsonObject activity;
    activity[QStringLiteral("state")] = QStringLiteral("Version %1").arg(m_version);
    activity[QStringLiteral("details")] = m_details.isEmpty() ? QStringLiteral("Playing Minecraft") : m_details;

    QJsonObject timestamps;
    timestamps[QStringLiteral("start")] = m_startTime > 0 ? m_startTime : QDateTime::currentSecsSinceEpoch();
    activity[QStringLiteral("timestamps")] = timestamps;

    QJsonObject assets;
    assets[QStringLiteral("large_image")] = QStringLiteral("minecraft");
    assets[QStringLiteral("large_text")] = Constants::LAUNCHER_NAME;
    activity[QStringLiteral("assets")] = assets;

    args[QStringLiteral("activity")] = activity;
    root[QStringLiteral("args")] = args;
    root[QStringLiteral("nonce")] = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    sendPacket(1, payload);
    emit presenceUpdated();
}

void DiscordRpc::onReadyRead() {
    while (m_socket->bytesAvailable() >= 8) {
        // Peek header
        const QByteArray header = m_socket->peek(8);
        quint32 opcode = 0;
        quint32 length = 0;
        QDataStream stream(header);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream >> opcode;
        stream >> length;

        if (m_socket->bytesAvailable() < 8 + length) {
            // Wait for rest of packet
            return;
        }

        // Consume header
        m_socket->read(8);
        const QByteArray payload = m_socket->read(length);

        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QString evt = obj.value(QStringLiteral("evt")).toString();
            const QString cmd = obj.value(QStringLiteral("cmd")).toString();

            if (evt == QStringLiteral("READY") || cmd == QStringLiteral("DISPATCH")) {
                m_handshakeCompleted = true;
                if (m_active && !m_version.isEmpty()) {
                    sendActivity();
                }
            }
        }
    }
}
