#include "mc/VersionManifest.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

VersionManifest::VersionManifest(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_cacheFilePath = Constants::defaultCacheDirectory() + QStringLiteral("/version_manifest_v2.json");
    connect(m_nam, &QNetworkAccessManager::finished, this, &VersionManifest::onReplyFinished);
}

void VersionManifest::setCacheFilePath(const QString &path) {
    m_cacheFilePath = path;
}

QString VersionManifest::cacheFilePath() const {
    return m_cacheFilePath;
}

bool VersionManifest::isLoaded() const {
    return m_isLoaded;
}

QList<VersionEntry> VersionManifest::allVersions() const {
    return m_manifestData.versions;
}

QList<VersionEntry> VersionManifest::filteredVersions(const QStringList &typeFilters) const {
    if (typeFilters.isEmpty()) {
        return m_manifestData.versions;
    }
    QList<VersionEntry> result;
    for (const auto &v : m_manifestData.versions) {
        if (typeFilters.contains(v.type, Qt::CaseInsensitive)) {
            result.append(v);
        }
    }
    return result;
}

QStringList VersionManifest::filteredVersionIds(const QStringList &typeFilters) const {
    const auto list = filteredVersions(typeFilters);
    QStringList ids;
    ids.reserve(list.size());
    for (const auto &v : list) {
        ids.append(v.id);
    }
    return ids;
}

std::optional<VersionEntry> VersionManifest::findVersion(const QString &versionId) const {
    for (const auto &v : m_manifestData.versions) {
        if (v.id == versionId) {
            return v;
        }
    }
    return std::nullopt;
}

QString VersionManifest::versionUrl(const QString &versionId) const {
    const auto entry = findVersion(versionId);
    return entry.has_value() ? entry->url : QString();
}

QString VersionManifest::latestRelease() const {
    return m_manifestData.latestRelease;
}

QString VersionManifest::latestSnapshot() const {
    return m_manifestData.latestSnapshot;
}

void VersionManifest::fetchManifest(bool forceRefresh, const QStringList &typeFilters) {
    m_activeFilters = typeFilters;

    const QString cachePath = m_cacheFilePath.isEmpty()
        ? (Constants::defaultCacheDirectory() + QStringLiteral("/version_manifest_v2.json"))
        : m_cacheFilePath;

    // Check if cache is still valid (1 hour / 3600 seconds)
    if (!forceRefresh && Utils::isCacheValid(cachePath, 3600)) {
        LOG_INFO(QStringLiteral("Loading version manifest from cache: %1").arg(cachePath));
        if (loadFromCache(cachePath, typeFilters)) {
            return;
        }
    }

    LOG_INFO(QStringLiteral("Fetching version manifest from %1").arg(Constants::VERSION_MANIFEST_URL));
    QNetworkRequest request((QUrl(Constants::VERSION_MANIFEST_URL)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    m_nam->get(request);
}

void VersionManifest::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();

    const QString cachePath = m_cacheFilePath.isEmpty()
        ? (Constants::defaultCacheDirectory() + QStringLiteral("/version_manifest_v2.json"))
        : m_cacheFilePath;

    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN(QStringLiteral("Network error fetching manifest: %1. Checking cache fallback...").arg(reply->errorString()));
        if (QFile::exists(cachePath) && loadFromCache(cachePath, m_activeFilters)) {
            LOG_INFO(QStringLiteral("Loaded version manifest from offline fallback cache."));
            return;
        }
        LOG_ERROR(QStringLiteral("Failed to load version manifest: %1").arg(reply->errorString()));
        emit errorOccurred(QStringLiteral("Failed to fetch version manifest: %1").arg(reply->errorString()));
        return;
    }

    const QByteArray data = reply->readAll();
    if (!parseJson(data, m_activeFilters, true)) {
        LOG_WARN(QStringLiteral("Failed to parse remote manifest JSON. Attempting cache fallback..."));
        if (QFile::exists(cachePath) && loadFromCache(cachePath, m_activeFilters)) {
            LOG_INFO(QStringLiteral("Loaded version manifest from offline fallback cache after parse error."));
            return;
        }
        LOG_ERROR(QStringLiteral("Corrupted version manifest JSON and no cache available."));
        emit errorOccurred(QStringLiteral("Corrupted version manifest JSON received from Mojang."));
    }
}

bool VersionManifest::loadFromCache(const QString &filePath, const QStringList &typeFilters) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return parseJson(data, typeFilters, false);
}

bool VersionManifest::parseJson(const QByteArray &data, const QStringList &typeFilters, bool saveToCache) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    VersionManifestData manifest;

    // Latest versions
    if (root.contains(QStringLiteral("latest")) && root[QStringLiteral("latest")].isObject()) {
        const QJsonObject latest = root[QStringLiteral("latest")].toObject();
        manifest.latestRelease = latest.value(QStringLiteral("release")).toString();
        manifest.latestSnapshot = latest.value(QStringLiteral("snapshot")).toString();
    }

    // Versions array
    const QJsonArray versionsArray = root.value(QStringLiteral("versions")).toArray();
    manifest.versions.reserve(versionsArray.size());

    for (const auto &val : versionsArray) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();

        VersionEntry entry;
        entry.id = obj.value(QStringLiteral("id")).toString();
        entry.type = obj.value(QStringLiteral("type")).toString();
        entry.url = obj.value(QStringLiteral("url")).toString();
        entry.time = obj.value(QStringLiteral("time")).toString();
        entry.releaseTime = obj.value(QStringLiteral("releaseTime")).toString();
        entry.sha1 = obj.value(QStringLiteral("sha1")).toString();
        entry.complianceLevel = obj.value(QStringLiteral("complianceLevel")).toInt(0);

        if (!entry.id.isEmpty()) {
            manifest.versions.append(entry);
        }
    }

    if (manifest.versions.isEmpty()) {
        return false;
    }

    m_manifestData = manifest;
    m_isLoaded = true;

    // Save to cache file if requested
    if (saveToCache && !m_cacheFilePath.isEmpty()) {
        Utils::ensureDirectoryExists(QFileInfo(m_cacheFilePath).absolutePath());
        QFile file(m_cacheFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(data);
            file.close();
        }
    }

    const auto filtered = filteredVersions(typeFilters);
    emit manifestDataLoaded(m_manifestData);
    emit manifestLoaded(filtered);
    return true;
}
