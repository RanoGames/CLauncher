#pragma once

#include <QObject>
#include <QString>
#include <QLocalSocket>
#include <QTimer>
#include <QDateTime>

class DiscordRpc : public QObject {
    Q_OBJECT

public:
    static DiscordRpc &instance();

    explicit DiscordRpc(QObject *parent = nullptr);
    ~DiscordRpc() override;

    // Start Rich Presence session with Minecraft version
    void start(const QString &version, const QString &details = QStringLiteral("Playing Minecraft"));

    // Update current activity
    void updatePresence(const QString &version, const QString &details = QStringLiteral("Playing Minecraft"));

    // Stop Rich Presence and close connection
    void stop();

    bool isConnected() const;
    bool isActive() const { return m_active; }

signals:
    void connected();
    void disconnected();
    void presenceUpdated();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QLocalSocket::LocalSocketError socketError);
    void tryNextPipe();
    void retryConnection();

private:
    void sendPacket(quint32 opcode, const QByteArray &jsonData);
    void sendHandshake();
    void sendActivity();
    QString getPipePath(int index) const;

    QLocalSocket *m_socket = nullptr;
    QString m_clientId;
    QString m_version;
    QString m_details;
    qint64 m_startTime = 0;
    int m_currentPipeIndex = 0;
    bool m_active = false;
    bool m_handshakeSent = false;
    bool m_handshakeCompleted = false;
    QTimer *m_retryTimer = nullptr;
};
