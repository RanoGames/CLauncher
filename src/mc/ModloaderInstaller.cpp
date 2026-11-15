#include "mc/ModloaderInstaller.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

ModloaderInstaller::ModloaderInstaller(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

ModloaderInstaller::~ModloaderInstaller() {
    cancel();
}

void ModloaderInstaller::cancel() {
    if (m_busy) {
        m_cancelled = true;
        if (m_currentReply) {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        m_busy = false;
        emit statusChanged(QStringLiteral("Installation cancelled."));
        emit error(QStringLiteral("Installation cancelled by user."));
    }
}

void ModloaderInstaller::fetchLoaders(const QString &gameVersion) {
    const QRegularExpression re(QStringLiteral(R"(1\.\d+(\.\d+)?)"));
    const auto match = re.match(gameVersion);
    const QString cleanVer = match.hasMatch() ? match.captured(0) : gameVersion.trimmed();

    if (cleanVer.isEmpty()) {
        emit error(QStringLiteral("Invalid Minecraft version specified."));
        return;
    }

    const QString urlStr = QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1").arg(cleanVer);
    LOG_INFO(QStringLiteral("Fetching Fabric loaders for Minecraft %1 from %2").arg(cleanVer, urlStr));

    QNetworkRequest req((QUrl(urlStr)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN(QStringLiteral("Failed to fetch Fabric loaders: %1").arg(reply->errorString()));
            emit loadersFetched(QStringList());
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        QStringList loaders;

        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const auto &item : arr) {
                if (item.isObject()) {
                    const QJsonObject obj = item.toObject();
                    if (obj.contains(QStringLiteral("loader")) && obj[QStringLiteral("loader")].isObject()) {
                        const QString v = obj[QStringLiteral("loader")].toObject().value(QStringLiteral("version")).toString();
                        if (!v.isEmpty() && !loaders.contains(v)) {
                            loaders.append(v);
                        }
                    }
                }
            }
        }

        emit loadersFetched(loaders);
    });
}

void ModloaderInstaller::installFabric(const QString &gameVersion,
                                       const QString &loaderVersion,
                                       const QString &customMinecraftDir) {
    if (m_busy) {
        emit error(QStringLiteral("Installer is already busy."));
        return;
    }

    const QRegularExpression re(QStringLiteral(R"(1\.\d+(\.\d+)?)"));
    const auto match = re.match(gameVersion);
    m_gameVersion = match.hasMatch() ? match.captured(0) : gameVersion.trimmed();
    m_loaderVersion = loaderVersion.trimmed();
    m_minecraftDir = customMinecraftDir.isEmpty() ? Constants::defaultMinecraftDirectory() : customMinecraftDir;
    m_busy = true;
    m_cancelled = false;

    if (m_gameVersion.isEmpty()) {
        m_busy = false;
        emit error(QStringLiteral("Invalid Minecraft version specified."));
        return;
    }

    emit statusChanged(QStringLiteral("Resolving Fabric loader for %1...").arg(m_gameVersion));
    emit progressPercent(10);

    if (m_loaderVersion.isEmpty()) {
        // Fetch loaders to find the latest stable version
        const QString urlStr = QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1").arg(m_gameVersion);
        QNetworkRequest req((QUrl(urlStr)));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(15000);

        m_currentReply = m_nam->get(req);
        connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
            if (m_currentReply) {
                onLoadersReplyFinished(m_currentReply);
            }
        });
    } else {
        fetchProfileJson(m_gameVersion, m_loaderVersion);
    }
}

void ModloaderInstaller::onLoadersReplyFinished(QNetworkReply *reply) {
    m_currentReply = nullptr;
    reply->deleteLater();

    if (m_cancelled) {
        m_busy = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to query Fabric meta API: %1").arg(reply->errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    QString selectedLoader;

    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        // Prefer first stable loader
        for (const auto &item : arr) {
            if (item.isObject()) {
                const QJsonObject obj = item.toObject();
                if (obj.contains(QStringLiteral("loader")) && obj[QStringLiteral("loader")].isObject()) {
                    const QJsonObject loaderObj = obj[QStringLiteral("loader")].toObject();
                    const bool stable = loaderObj.value(QStringLiteral("stable")).toBool(false);
                    const QString ver = loaderObj.value(QStringLiteral("version")).toString();
                    if (stable && !ver.isEmpty()) {
                        selectedLoader = ver;
                        break;
                    }
                }
            }
        }

        // Fallback to first available if none marked stable
        if (selectedLoader.isEmpty() && !arr.isEmpty() && arr.first().isObject()) {
            const QJsonObject firstObj = arr.first().toObject();
            if (firstObj.contains(QStringLiteral("loader")) && firstObj[QStringLiteral("loader")].isObject()) {
                selectedLoader = firstObj[QStringLiteral("loader")].toObject().value(QStringLiteral("version")).toString();
            }
        }
    }

    if (selectedLoader.isEmpty()) {
        m_busy = false;
        const QString err = QStringLiteral("No compatible Fabric loader found for Minecraft %1").arg(m_gameVersion);
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    m_loaderVersion = selectedLoader;
    LOG_INFO(QStringLiteral("Selected Fabric loader %1 for Minecraft %2").arg(m_loaderVersion, m_gameVersion));
    fetchProfileJson(m_gameVersion, m_loaderVersion);
}

void ModloaderInstaller::fetchProfileJson(const QString &gameVersion, const QString &loaderVersion) {
    if (m_cancelled) return;

    emit statusChanged(QStringLiteral("Downloading Fabric profile (%1, loader %2)...").arg(gameVersion, loaderVersion));
    emit progressPercent(40);

    const QString urlStr = QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                               .arg(gameVersion, loaderVersion);
    LOG_INFO(QStringLiteral("Fetching Fabric profile JSON from %1").arg(urlStr));

    QNetworkRequest req((QUrl(urlStr)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        if (m_currentReply) {
            onProfileJsonReplyFinished(m_currentReply);
        }
    });
}

void ModloaderInstaller::onProfileJsonReplyFinished(QNetworkReply *reply) {
    m_currentReply = nullptr;
    reply->deleteLater();

    if (m_cancelled) {
        m_busy = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to download Fabric version definition: %1").arg(reply->errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_busy = false;
        const QString err = QStringLiteral("Fabric returned invalid JSON: %1").arg(parseError.errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    emit progressPercent(70);
    emit statusChanged(QStringLiteral("Saving Fabric version configuration..."));

    const QJsonObject obj = doc.object();
    QString versionId = obj.value(QStringLiteral("id")).toString();
    if (versionId.isEmpty()) {
        versionId = QStringLiteral("fabric-loader-%1-%2").arg(m_loaderVersion, m_gameVersion);
    }

    saveVersionJson(versionId, data);
}

void ModloaderInstaller::saveVersionJson(const QString &versionId, const QByteArray &jsonData) {
    const QString versionsDir = QDir(m_minecraftDir).filePath(QStringLiteral("versions"));
    const QString targetDir = QDir(versionsDir).filePath(versionId);
    Utils::ensureDirectoryExists(targetDir);

    const QString targetJsonPath = QDir(targetDir).filePath(QStringLiteral("%1.json").arg(versionId));
    LOG_INFO(QStringLiteral("Writing version JSON to %1").arg(targetJsonPath));

    QSaveFile saveFile(targetJsonPath);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to open %1 for writing: %2").arg(targetJsonPath, saveFile.errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    saveFile.write(jsonData);
    if (!saveFile.commit()) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to commit version JSON: %1").arg(saveFile.errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    m_busy = false;
    LOG_INFO(QStringLiteral("Fabric %1 installed successfully for Minecraft %2").arg(versionId, m_gameVersion));
    emit progressPercent(100);
    emit statusChanged(QStringLiteral("Fabric (%1) installed successfully!").arg(versionId));
    emit installFinished(versionId);
}
