#include "mc/ModBrowser.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>

ModBrowser::ModBrowser(QObject *parent)
    : QObject(parent)
    , m_netManager(new QNetworkAccessManager(this)) {
}

ModBrowser::~ModBrowser() {
    cancelSearch();
    cancelDownloads();
}

QString ModBrowser::sanitizeVersion(const QString &version) {
    static const QRegularExpression verRe(QStringLiteral(R"(1\.\d+(?:\.\d+)?)"));
    auto match = verRe.match(version);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    return version.trimmed();
}

void ModBrowser::cancelSearch() {
    if (m_searchReply) {
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }
    m_isSearching = false;
}

void ModBrowser::cancelDownloads() {
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.key()) {
            it.key()->abort();
            it.key()->deleteLater();
        }
        if (it.value().file) {
            it.value().file->close();
            delete it.value().file;
        }
    }
    m_activeDownloads.clear();

    for (auto it = m_pendingInstalls.begin(); it != m_pendingInstalls.end(); ++it) {
        if (it.key()) {
            it.key()->abort();
            it.key()->deleteLater();
        }
    }
    m_pendingInstalls.clear();
}

void ModBrowser::searchMods(const QString &query,
                           const QString &gameVersion,
                           const QString &loader,
                           int limit,
                           int offset) {
    cancelSearch();
    m_isSearching = true;
    emit searchStarted();

    QString cleanVer = sanitizeVersion(gameVersion);
    QString cleanLoader = loader.trimmed().toLower();

    QUrl url(Constants::MODRINTH_API_URL + QStringLiteral("/search"));
    QUrlQuery queryParams;

    if (!query.trimmed().isEmpty()) {
        queryParams.addQueryItem(QStringLiteral("query"), query.trimmed());
    }

    queryParams.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 100)));
    if (offset > 0) {
        queryParams.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    }

    // Facets JSON array: [ ["project_type:mod"], ["versions:<ver>"], ["categories:<loader>"] ]
    QJsonArray facets;
    QJsonArray typeFacet;
    typeFacet.append(QStringLiteral("project_type:mod"));
    facets.append(typeFacet);

    if (!cleanVer.isEmpty()) {
        QJsonArray verFacet;
        verFacet.append(QStringLiteral("versions:%1").arg(cleanVer));
        facets.append(verFacet);
    }

    if (!cleanLoader.isEmpty() && cleanLoader != QStringLiteral("all")) {
        QJsonArray loaderFacet;
        loaderFacet.append(QStringLiteral("categories:%1").arg(cleanLoader));
        facets.append(loaderFacet);
    }

    queryParams.addQueryItem(QStringLiteral("facets"), QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    url.setQuery(queryParams);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1 (contact: github.com/RanoGames/CLauncher)").arg(Constants::LAUNCHER_VERSION));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);

    LOG_INFO(QStringLiteral("Modrinth search request: %1").arg(url.toString()));
    m_searchReply = m_netManager->get(request);
    connect(m_searchReply, &QNetworkReply::finished, this, &ModBrowser::onSearchReplyFinished);
}

void ModBrowser::onSearchReplyFinished() {
    if (!m_searchReply) {
        m_isSearching = false;
        return;
    }

    QNetworkReply *reply = m_searchReply;
    m_searchReply = nullptr;
    m_isSearching = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_WARN(QStringLiteral("Modrinth search failed: %1").arg(err));
        emit searchError(err);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        QString err = QStringLiteral("Failed to parse Modrinth search response: %1").arg(parseErr.errorString());
        LOG_WARN(err);
        emit searchError(err);
        return;
    }

    QJsonObject root = doc.object();
    int totalHits = root[QStringLiteral("total_hits")].toInt(0);
    QJsonArray hits = root[QStringLiteral("hits")].toArray();

    QList<ModSearchResult> results;
    for (const QJsonValue &val : hits) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        ModSearchResult res;
        res.projectId = obj[QStringLiteral("project_id")].toString();
        res.slug = obj[QStringLiteral("slug")].toString();
        res.title = obj[QStringLiteral("title")].toString();
        res.description = obj[QStringLiteral("description")].toString();
        res.author = obj[QStringLiteral("author")].toString();
        res.downloads = obj[QStringLiteral("downloads")].toInteger();
        res.follows = obj[QStringLiteral("follows")].toInteger();
        res.iconUrl = obj[QStringLiteral("icon_url")].toString();

        QJsonArray cats = obj[QStringLiteral("categories")].toArray();
        for (const auto &c : cats) res.categories.append(c.toString());

        QJsonArray vers = obj[QStringLiteral("versions")].toArray();
        for (const auto &v : vers) res.versions.append(v.toString());

        results.append(res);
    }

    LOG_INFO(QStringLiteral("Modrinth search found %1 results").arg(results.size()));
    emit searchFinished(results, totalHits);
}

void ModBrowser::fetchProjectVersions(const QString &projectId,
                                     const QString &gameVersion,
                                     const QString &loader) {
    if (projectId.isEmpty()) {
        return;
    }

    QString cleanVer = sanitizeVersion(gameVersion);
    QString cleanLoader = loader.trimmed().toLower();

    QUrl url(QStringLiteral("%1/project/%2/version").arg(Constants::MODRINTH_API_URL, projectId));
    QUrlQuery queryParams;

    if (!cleanLoader.isEmpty() && cleanLoader != QStringLiteral("all")) {
        QJsonArray lArr;
        lArr.append(cleanLoader);
        queryParams.addQueryItem(QStringLiteral("loaders"), QString::fromUtf8(QJsonDocument(lArr).toJson(QJsonDocument::Compact)));
    }

    if (!cleanVer.isEmpty()) {
        QJsonArray vArr;
        vArr.append(cleanVer);
        queryParams.addQueryItem(QStringLiteral("game_versions"), QString::fromUtf8(QJsonDocument(vArr).toJson(QJsonDocument::Compact)));
    }

    if (!queryParams.isEmpty()) {
        url.setQuery(queryParams);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1").arg(Constants::LAUNCHER_VERSION));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);

    LOG_INFO(QStringLiteral("Fetching versions for project %1: %2").arg(projectId, url.toString()));
    QNetworkReply *reply = m_netManager->get(request);
    reply->setProperty("projectId", projectId);
    connect(reply, &QNetworkReply::finished, this, &ModBrowser::onVersionsReplyFinished);
}

void ModBrowser::onVersionsReplyFinished() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    QString projectId = reply->property("projectId").toString();

    // Check if this was triggered as part of installMod
    bool isPendingInstall = m_pendingInstalls.contains(reply);
    PendingInstall installData;
    if (isPendingInstall) {
        installData = m_pendingInstalls.take(reply);
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_WARN(QStringLiteral("Failed to fetch versions for project %1: %2").arg(projectId, err));
        if (isPendingInstall) {
            emit downloadError(installData.modTitle, err);
        } else {
            emit versionsError(projectId, err);
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        QString err = QStringLiteral("Failed to parse project versions JSON: %1").arg(parseErr.errorString());
        LOG_WARN(err);
        if (isPendingInstall) {
            emit downloadError(installData.modTitle, err);
        } else {
            emit versionsError(projectId, err);
        }
        return;
    }

    QJsonArray verList = doc.array();
    QList<ModVersionInfo> versions;

    for (const QJsonValue &val : verList) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        ModVersionInfo vInfo;
        vInfo.id = obj[QStringLiteral("id")].toString();
        vInfo.name = obj[QStringLiteral("name")].toString();
        vInfo.versionNumber = obj[QStringLiteral("version_number")].toString();

        QJsonArray gv = obj[QStringLiteral("game_versions")].toArray();
        for (const auto &g : gv) vInfo.gameVersions.append(g.toString());

        QJsonArray ld = obj[QStringLiteral("loaders")].toArray();
        for (const auto &l : ld) vInfo.loaders.append(l.toString());

        QJsonArray files = obj[QStringLiteral("files")].toArray();
        for (const auto &f : files) {
            if (!f.isObject()) continue;
            QJsonObject fObj = f.toObject();
            ModVersionFile fileInfo;
            fileInfo.url = fObj[QStringLiteral("url")].toString();
            fileInfo.filename = fObj[QStringLiteral("filename")].toString();
            fileInfo.size = fObj[QStringLiteral("size")].toInteger();
            fileInfo.primary = fObj[QStringLiteral("primary")].toBool(false);
            QJsonObject hashes = fObj[QStringLiteral("hashes")].toObject();
            fileInfo.sha1 = hashes[QStringLiteral("sha1")].toString();
            fileInfo.sha512 = hashes[QStringLiteral("sha512")].toString();
            vInfo.files.append(fileInfo);
        }

        versions.append(vInfo);
    }

    emit versionsLoaded(projectId, versions);

    if (isPendingInstall) {
        if (versions.isEmpty()) {
            emit downloadError(installData.modTitle, QStringLiteral("Не найдено совместимых версий мода для выбранной конфигурации."));
            return;
        }

        // Pick primary file or first available file from most recent version
        ModVersionFile targetFile;
        bool found = false;

        for (const ModVersionInfo &ver : versions) {
            for (const ModVersionFile &f : ver.files) {
                if (f.primary) {
                    targetFile = f;
                    found = true;
                    break;
                }
            }
            if (found) break;
            if (!ver.files.isEmpty()) {
                targetFile = ver.files.first();
                found = true;
                break;
            }
        }

        if (!found || targetFile.url.isEmpty()) {
            emit downloadError(installData.modTitle, QStringLiteral("В релизе мода не обнаружено файлов для скачивания."));
            return;
        }

        emit statusMessage(QStringLiteral("Скачивание %1 (%2)...").arg(targetFile.filename, installData.modTitle));
        downloadFile(targetFile.url, targetFile.filename, installData.targetModsDir);
    }
}

void ModBrowser::installMod(const QString &projectId,
                           const QString &modTitle,
                           const QString &gameVersion,
                           const QString &loader,
                           const QString &targetModsDir) {
    if (projectId.isEmpty()) return;

    QString cleanVer = sanitizeVersion(gameVersion);
    QString cleanLoader = loader.trimmed().toLower();

    QUrl url(QStringLiteral("%1/project/%2/version").arg(Constants::MODRINTH_API_URL, projectId));
    QUrlQuery queryParams;

    if (!cleanLoader.isEmpty() && cleanLoader != QStringLiteral("all")) {
        QJsonArray lArr;
        lArr.append(cleanLoader);
        queryParams.addQueryItem(QStringLiteral("loaders"), QString::fromUtf8(QJsonDocument(lArr).toJson(QJsonDocument::Compact)));
    }

    if (!cleanVer.isEmpty()) {
        QJsonArray vArr;
        vArr.append(cleanVer);
        queryParams.addQueryItem(QStringLiteral("game_versions"), QString::fromUtf8(QJsonDocument(vArr).toJson(QJsonDocument::Compact)));
    }

    if (!queryParams.isEmpty()) {
        url.setQuery(queryParams);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1").arg(Constants::LAUNCHER_VERSION));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);

    emit statusMessage(QStringLiteral("Поиск совместимого файла %1...").arg(modTitle));
    QNetworkReply *reply = m_netManager->get(request);
    reply->setProperty("projectId", projectId);

    PendingInstall pending;
    pending.projectId = projectId;
    pending.modTitle = modTitle;
    pending.targetModsDir = targetModsDir;
    m_pendingInstalls.insert(reply, pending);

    connect(reply, &QNetworkReply::finished, this, &ModBrowser::onVersionsReplyFinished);
}

void ModBrowser::downloadFile(const QString &fileUrl,
                             const QString &fileName,
                             const QString &targetModsDir) {
    if (fileUrl.isEmpty() || fileName.isEmpty()) {
        emit downloadError(fileName, QStringLiteral("Invalid download parameters."));
        return;
    }

    QString modsDir = targetModsDir.isEmpty() ? Constants::defaultModsDirectory() : targetModsDir;
    Utils::ensureDirectoryExists(modsDir);
    QString targetFilePath = QDir(modsDir).filePath(fileName);

    auto *file = new QFile(targetFilePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString err = QStringLiteral("Failed to open file for writing: %1").arg(targetFilePath);
        LOG_ERROR(err);
        delete file;
        emit downloadError(fileName, err);
        return;
    }

    QNetworkRequest request((QUrl(fileUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CLauncher/%1").arg(Constants::LAUNCHER_VERSION));
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_netManager->get(request);

    ActiveDownload active;
    active.fileName = fileName;
    active.targetPath = targetFilePath;
    active.file = file;
    m_activeDownloads.insert(reply, active);

    emit downloadStarted(fileName);

    connect(reply, &QNetworkReply::downloadProgress, this, &ModBrowser::onDownloadProgress);
    connect(reply, &QIODevice::readyRead, this, &ModBrowser::onDownloadReadyRead);
    connect(reply, &QNetworkReply::finished, this, &ModBrowser::onDownloadReplyFinished);
}

void ModBrowser::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    QString fileName = m_activeDownloads[reply].fileName;
    int percent = (bytesTotal > 0) ? static_cast<int>((bytesReceived * 100) / bytesTotal) : 0;
    emit downloadProgress(fileName, bytesReceived, bytesTotal, percent);
}

void ModBrowser::onDownloadReadyRead() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    QFile *file = m_activeDownloads[reply].file;
    if (file && file->isOpen()) {
        file->write(reply->readAll());
    }
}

void ModBrowser::onDownloadReplyFinished() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    ActiveDownload active = m_activeDownloads.take(reply);
    reply->deleteLater();

    if (active.file) {
        if (reply->bytesAvailable() > 0) {
            active.file->write(reply->readAll());
        }
        active.file->flush();
        active.file->close();
        delete active.file;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        LOG_ERROR(QStringLiteral("Mod download failed (%1): %2").arg(active.fileName, err));
        QFile::remove(active.targetPath);
        emit downloadError(active.fileName, err);
        return;
    }

    LOG_INFO(QStringLiteral("Mod downloaded successfully to %1").arg(active.targetPath));
    emit downloadFinished(active.fileName, active.targetPath);
}
