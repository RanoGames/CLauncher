#include "Updater.h"
#include "Constants.h"
#include "Utils.h"
#include "Logger.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_netManager(new QNetworkAccessManager(this)) {
}

Updater::~Updater() {
    abort();
}

void Updater::abort() {
    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }
    m_isChecking = false;

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    m_isDownloading = false;
}

void Updater::checkForUpdates(const QString &apiUrl) {
    if (m_isChecking) {
        return;
    }

    abort();
    m_isChecking = true;
    emit checkingStarted();

    QString urlStr = apiUrl.isEmpty() ? Constants::GITHUB_API_URL : apiUrl;
    QUrl url(urlStr);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1 (contact: github.com/RanoGames/CLauncher)").arg(Constants::LAUNCHER_VERSION));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setTransferTimeout(15000);

    LOG_INFO(QStringLiteral("Checking for updates at %1...").arg(urlStr));
    m_checkReply = m_netManager->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, &Updater::onCheckFinished);
}

void Updater::onCheckFinished() {
    if (!m_checkReply) {
        m_isChecking = false;
        return;
    }

    QNetworkReply *reply = m_checkReply;
    m_checkReply = nullptr;
    m_isChecking = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_WARN(QStringLiteral("Update check failed: %1").arg(err));
        emit checkFailed(err);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        QString err = QStringLiteral("Failed to parse update manifest JSON: %1").arg(parseErr.errorString());
        LOG_WARN(err);
        emit checkFailed(err);
        return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj[QStringLiteral("tag_name")].toString().trimmed();
    QString cleanVersion = tagName;
    if (cleanVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        cleanVersion = cleanVersion.mid(1);
    }

    if (cleanVersion.isEmpty()) {
        QString err = QStringLiteral("No version tag found in release response.");
        LOG_WARN(err);
        emit checkFailed(err);
        return;
    }

    UpdateInfo info;
    info.currentVersion = Constants::LAUNCHER_VERSION;
    info.latestVersion = cleanVersion;
    info.changelog = obj[QStringLiteral("body")].toString();
    info.releaseUrl = obj[QStringLiteral("html_url")].toString();

    // Check assets
    QJsonArray assets = obj[QStringLiteral("assets")].toArray();
    for (const QJsonValue &val : assets) {
        if (!val.isObject()) continue;
        QJsonObject assetObj = val.toObject();
        QString assetName = assetObj[QStringLiteral("name")].toString();
        QString downloadUrl = assetObj[QStringLiteral("browser_download_url")].toString();
        qint64 size = assetObj[QStringLiteral("size")].toInteger();

        // Platform preference: on Windows prefer .exe, on Linux prefer .AppImage or .tar.gz
#if defined(Q_OS_WIN)
        if (assetName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            info.downloadUrl = downloadUrl;
            info.assetName = assetName;
            info.assetSize = size;
            break;
        }
#else
        if (assetName.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive) ||
            assetName.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive)) {
            info.downloadUrl = downloadUrl;
            info.assetName = assetName;
            info.assetSize = size;
            break;
        }
#endif
        if (info.downloadUrl.isEmpty() && !downloadUrl.isEmpty()) {
            info.downloadUrl = downloadUrl;
            info.assetName = assetName;
            info.assetSize = size;
        }
    }

    info.updateAvailable = Utils::isVersionNewer(info.latestVersion, info.currentVersion);
    m_lastInfo = info;

    if (info.updateAvailable) {
        LOG_INFO(QStringLiteral("Update available! Current: %1, Latest: %2").arg(info.currentVersion, info.latestVersion));
        emit updateAvailable(info);
    } else {
        LOG_INFO(QStringLiteral("Launcher is up to date (version %1)").arg(info.currentVersion));
        emit noUpdateAvailable(info.currentVersion);
    }
}

void Updater::downloadUpdate(const QString &downloadUrl, const QString &destinationFilePath) {
    if (m_isDownloading) {
        return;
    }

    if (downloadUrl.isEmpty()) {
        emit downloadFailed(QStringLiteral("Empty download URL."));
        return;
    }

    abort();
    m_isDownloading = true;

    QString targetPath = destinationFilePath;
    if (targetPath.isEmpty()) {
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString filename = QFileInfo(QUrl(downloadUrl).path()).fileName();
        if (filename.isEmpty()) {
            filename = QStringLiteral("CLauncher_update");
        }
        targetPath = QDir(tempDir).filePath(filename);
    }

    m_downloadTarget = targetPath;
    m_downloadFile = new QFile(m_downloadTarget);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString err = QStringLiteral("Failed to open file for writing update: %1").arg(m_downloadTarget);
        LOG_ERROR(err);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        m_isDownloading = false;
        emit downloadFailed(err);
        return;
    }

    QNetworkRequest request((QUrl(downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1").arg(Constants::LAUNCHER_VERSION));
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    LOG_INFO(QStringLiteral("Downloading update from %1 to %2...").arg(downloadUrl, m_downloadTarget));
    m_downloadReply = m_netManager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &Updater::onDownloadProgress);
    connect(m_downloadReply, &QIODevice::readyRead, this, &Updater::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished, this, &Updater::onDownloadFinished);
}

void Updater::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    int percent = (bytesTotal > 0) ? static_cast<int>((bytesReceived * 100) / bytesTotal) : 0;
    emit downloadProgress(bytesReceived, bytesTotal, percent);
}

void Updater::onDownloadReadyRead() {
    if (m_downloadReply && m_downloadFile && m_downloadFile->isOpen()) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void Updater::onDownloadFinished() {
    if (!m_downloadReply) {
        return;
    }

    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    m_isDownloading = false;
    reply->deleteLater();

    if (m_downloadFile) {
        if (reply->bytesAvailable() > 0) {
            m_downloadFile->write(reply->readAll());
        }
        m_downloadFile->flush();
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_ERROR(QStringLiteral("Update download failed: %1").arg(err));
        QFile::remove(m_downloadTarget);
        emit downloadFailed(err);
        return;
    }

    LOG_INFO(QStringLiteral("Update downloaded successfully: %1").arg(m_downloadTarget));
    emit downloadFinished(m_downloadTarget);
}
