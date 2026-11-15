#include "mc/SkinManager.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QNetworkRequest>
#include <QUrl>

SkinManager &SkinManager::instance() {
    static SkinManager s_instance;
    return s_instance;
}

SkinManager::SkinManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString SkinManager::getSkinsCacheDir(const QString &customMinecraftDir) const {
    const QString base = customMinecraftDir.isEmpty() ? Constants::defaultCacheDirectory() : QDir(customMinecraftDir).filePath(QStringLiteral("cache"));
    const QString skinsDir = QDir(base).filePath(QStringLiteral("skins"));
    Utils::ensureDirectoryExists(skinsDir);
    return skinsDir;
}

QPixmap SkinManager::defaultAvatar(int size) {
    static const quint32 stevePixels[8][8] = {
        {0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18},
        {0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18, 0xFF281D18},
        {0xFF281D18, 0xFFB78262, 0xFFB78262, 0xFFB78262, 0xFFB78262, 0xFFB78262, 0xFFB78262, 0xFF281D18},
        {0xFFB78262, 0xFFB78262, 0xFFFFFFFF, 0xFF2D3282, 0xFF2D3282, 0xFFFFFFFF, 0xFFB78262, 0xFFB78262},
        {0xFFB78262, 0xFFB78262, 0xFF945B3F, 0xFF945B3F, 0xFF945B3F, 0xFF945B3F, 0xFFB78262, 0xFFB78262},
        {0xFFB78262, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFFB78262},
        {0xFFB78262, 0xFFB78262, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFFB78262, 0xFFB78262},
        {0xFFB78262, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFF663C26, 0xFFB78262}
    };

    QImage img(8, 8, QImage::Format_ARGB32);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            img.setPixelColor(x, y, QColor::fromRgba(stevePixels[y][x]));
        }
    }
    return QPixmap::fromImage(img.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

QPixmap SkinManager::extractFace(const QImage &skinImage, int size) {
    if (skinImage.isNull() || skinImage.width() < 16 || skinImage.height() < 16) {
        return defaultAvatar(size);
    }

    const QImage converted = skinImage.convertToFormat(QImage::Format_ARGB32);
    const QImage face = converted.copy(8, 8, 8, 8);

    QImage combined(8, 8, QImage::Format_ARGB32);
    combined.fill(Qt::transparent);

    QPainter painter(&combined);
    painter.drawImage(0, 0, face);

    // Overlay helmet layer (x=40, y=8, w=8, h=8)
    if (converted.width() >= 48 && converted.height() >= 16) {
        const QImage helmet = converted.copy(40, 8, 8, 8);

        bool hasTransparent = false;
        bool allSameColor = true;
        const QRgb firstPixel = helmet.pixel(0, 0);

        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const QRgb p = helmet.pixel(x, y);
                if (qAlpha(p) < 255) {
                    hasTransparent = true;
                }
                if (p != firstPixel) {
                    allSameColor = false;
                }
            }
        }

        // Overlay helmet if it contains legitimate transparency or variation
        if (hasTransparent && !allSameColor) {
            painter.drawImage(0, 0, helmet);
        }
    }
    painter.end();

    const QImage scaled = combined.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    return QPixmap::fromImage(scaled);
}

QPixmap SkinManager::getAvatar(const QString &username, int size, const QString &customMinecraftDir) {
    const QString cleanUser = username.trimmed();
    if (cleanUser.isEmpty()) {
        return defaultAvatar(size);
    }

    const QString cacheKey = QStringLiteral("%1_%2").arg(cleanUser.toLower()).arg(size);
    if (m_memoryCache.contains(cacheKey)) {
        return m_memoryCache.value(cacheKey);
    }

    const QString skinsDir = getSkinsCacheDir(customMinecraftDir);
    const QString avatarPath = QDir(skinsDir).filePath(QStringLiteral("%1_%2.png").arg(cleanUser.toLower()).arg(size));

    if (QFile::exists(avatarPath)) {
        QPixmap pix(avatarPath);
        if (!pix.isNull()) {
            m_memoryCache.insert(cacheKey, pix);
            return pix;
        }
    }

    const QString skinPath = QDir(skinsDir).filePath(QStringLiteral("%1.png").arg(cleanUser.toLower()));
    if (QFile::exists(skinPath)) {
        QImage skinImg(skinPath);
        if (!skinImg.isNull()) {
            const QPixmap avatar = extractFace(skinImg, size);
            avatar.save(avatarPath, "PNG");
            m_memoryCache.insert(cacheKey, avatar);
            return avatar;
        }
    }

    // Not found in cache; kick off async download and return default Steve in the meantime
    loadAvatarAsync(cleanUser, size, customMinecraftDir);
    return defaultAvatar(size);
}

void SkinManager::loadAvatarAsync(const QString &username, int size, const QString &customMinecraftDir) {
    const QString cleanUser = username.trimmed();
    if (cleanUser.isEmpty()) return;

    if (m_activeRequests.contains(cleanUser.toLower())) {
        return;
    }

    m_activeRequests.insert(cleanUser.toLower());
    tryFetchSkin(cleanUser, size, 0, customMinecraftDir);
}

void SkinManager::tryFetchSkin(const QString &username, int size, int attemptIndex, const QString &customMinecraftDir) {
    QString urlStr;
    bool isDirectAvatar = false;

    switch (attemptIndex) {
    case 0:
        // 1. Ely.by skin
        urlStr = QStringLiteral("http://skinsystem.ely.by/textures/%1.png").arg(username);
        break;
    case 1:
        // 2. Minotar full skin
        urlStr = QStringLiteral("https://minotar.net/skin/%1").arg(username);
        break;
    case 2:
        // 3. Crafatar full skin
        urlStr = QStringLiteral("https://crafatar.com/skins/%1").arg(username);
        break;
    case 3:
        // 4. Minotar direct avatar fallback
        urlStr = QStringLiteral("https://minotar.net/avatar/%1/%2.png").arg(username).arg(size);
        isDirectAvatar = true;
        break;
    default:
        // All attempts failed
        m_activeRequests.remove(username.toLower());
        LOG_WARN(QStringLiteral("Could not load skin or avatar for %1 from any source").arg(username));
        emit fetchFailed(username, QStringLiteral("Skin not found"));
        return;
    }

    LOG_DEBUG(QStringLiteral("Fetching skin/avatar for %1 from %2 (attempt %3)").arg(username, urlStr).arg(attemptIndex));

    QNetworkRequest req((QUrl(urlStr)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(req);
    reply->setProperty("username", username);
    reply->setProperty("size", size);
    reply->setProperty("attempt", attemptIndex);
    reply->setProperty("directAvatar", isDirectAvatar);
    reply->setProperty("mcDir", customMinecraftDir);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void SkinManager::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();

    const QString username = reply->property("username").toString();
    const int size = reply->property("size").toInt();
    const int attempt = reply->property("attempt").toInt();
    const bool isDirectAvatar = reply->property("directAvatar").toBool();
    const QString customDir = reply->property("mcDir").toString();

    if (reply->error() != QNetworkReply::NoError || reply->bytesAvailable() == 0) {
        // Try next provider
        tryFetchSkin(username, size, attempt + 1, customDir);
        return;
    }

    const QByteArray data = reply->readAll();
    QImage loadedImage;
    if (!loadedImage.loadFromData(data)) {
        tryFetchSkin(username, size, attempt + 1, customDir);
        return;
    }

    const QString skinsDir = getSkinsCacheDir(customDir);
    const QString cacheKey = QStringLiteral("%1_%2").arg(username.toLower()).arg(size);
    QPixmap avatarPix;

    if (isDirectAvatar) {
        avatarPix = QPixmap::fromImage(loadedImage.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation));
        const QString avatarPath = QDir(skinsDir).filePath(QStringLiteral("%1_%2.png").arg(username.toLower()).arg(size));
        avatarPix.save(avatarPath, "PNG");
    } else {
        // Full skin downloaded: save skin file and extract avatar
        const QString skinPath = QDir(skinsDir).filePath(QStringLiteral("%1.png").arg(username.toLower()));
        QFile f(skinPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }

        avatarPix = extractFace(loadedImage, size);
        const QString avatarPath = QDir(skinsDir).filePath(QStringLiteral("%1_%2.png").arg(username.toLower()).arg(size));
        avatarPix.save(avatarPath, "PNG");

        emit skinLoaded(username, loadedImage);
    }

    m_memoryCache.insert(cacheKey, avatarPix);
    m_activeRequests.remove(username.toLower());

    LOG_INFO(QStringLiteral("Skin avatar successfully cached for %1").arg(username));
    emit avatarLoaded(username, avatarPix);
}
