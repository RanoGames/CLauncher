#pragma once

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QImage>
#include <QMap>
#include <QSet>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class SkinManager : public QObject {
    Q_OBJECT

public:
    static SkinManager &instance();

    explicit SkinManager(QObject *parent = nullptr);
    ~SkinManager() override = default;

    // Get avatar pixmap (returns immediately: from cache, or default Steve if pending fetch)
    QPixmap getAvatar(const QString &username, int size = 48, const QString &customMinecraftDir = QString());

    // Trigger async fetch of skin and avatar
    void loadAvatarAsync(const QString &username, int size = 48, const QString &customMinecraftDir = QString());

    // Extract face + helmet overlay from skin texture
    static QPixmap extractFace(const QImage &skinImage, int size = 48);

    // Default Steve avatar pixmap
    static QPixmap defaultAvatar(int size = 48);

signals:
    void avatarLoaded(const QString &username, const QPixmap &avatar);
    void skinLoaded(const QString &username, const QImage &skin);
    void fetchFailed(const QString &username, const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString getSkinsCacheDir(const QString &customMinecraftDir = QString()) const;
    void tryFetchSkin(const QString &username, int size, int attemptIndex, const QString &customMinecraftDir);

    QNetworkAccessManager *m_nam = nullptr;
    QMap<QString, QPixmap> m_memoryCache;
    QSet<QString> m_activeRequests;
};
