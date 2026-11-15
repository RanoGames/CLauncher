#include "mc/Downloader.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"
#include "core/Settings.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>

Downloader::Downloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_minecraftDir = Constants::defaultMinecraftDirectory();
}

Downloader::~Downloader() {
    cancel();
}

bool Downloader::isBusy() const {
    return m_busy;
}

void Downloader::cancel() {
    if (!m_busy || m_cancelled) {
        return;
    }
    m_cancelled = true;
    LOG_INFO(QStringLiteral("Cancelling active downloads..."));

    // Abort active network replies
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.key()) {
            it.key()->abort();
            it.key()->deleteLater();
        }
        if (!it.value().tempFilePath.isEmpty()) {
            QFile::remove(it.value().tempFilePath);
        }
    }
    m_activeDownloads.clear();
    m_queue.clear();
    m_busy = false;

    emit statusChanged(QStringLiteral("Download cancelled."));
    emit error(QStringLiteral("Download cancelled by user."));
}

void Downloader::downloadVersion(const QString &versionId, const QString &customMinecraftDir) {
    if (m_busy) {
        LOG_WARN(QStringLiteral("Downloader is already busy."));
        return;
    }

    m_busy = true;
    m_cancelled = false;
    m_targetVersionId = versionId;
    m_minecraftDir = customMinecraftDir.isEmpty() ? Constants::defaultMinecraftDirectory() : customMinecraftDir;
    m_queue.clear();
    m_activeDownloads.clear();
    m_nativeJarsToExtract.clear();
    m_totalBytes = 0;
    m_completedBytes = 0;
    m_totalTasks = 0;
    m_completedTasks = 0;

    LOG_INFO(QStringLiteral("Starting download for version %1 in %2").arg(versionId, m_minecraftDir));
    emit statusChanged(QStringLiteral("Preparing download for %1...").arg(versionId));
    emit downloadProgress(0, 100);
    emit progressPercent(0);

    startDownloadChain(versionId);
}

void Downloader::startDownloadChain(const QString &versionId) {
    const QString versionJsonPath = QDir(m_minecraftDir).filePath(
        QStringLiteral("versions/%1/%1.json").arg(versionId));

    if (QFile::exists(versionJsonPath)) {
        LOG_INFO(QStringLiteral("Found existing version JSON for %1 at %2").arg(versionId, versionJsonPath));
        QFile file(versionJsonPath);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray data = file.readAll();
            file.close();
            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
            if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
                handleVersionJsonDownloaded(versionId, data);
                return;
            } else {
                LOG_WARN(QStringLiteral("Existing version JSON for %1 is corrupted (%2), removing and re-fetching...")
                             .arg(versionId, parseErr.errorString()));
                QFile::remove(versionJsonPath);
            }
        }
    }

    // Try finding version URL from cache/manifest
    emit statusChanged(QStringLiteral("Fetching version metadata for %1...").arg(versionId));
    const QString cacheDir = Settings::instance().effectiveGameDir() + QStringLiteral("/cache");
    Utils::ensureDirectoryExists(cacheDir);
    const QString manifestPath = cacheDir + QStringLiteral("/version_manifest_v2.json");
    QString jsonUrl;

    if (QFile::exists(manifestPath)) {
        QFile mfile(manifestPath);
        if (mfile.open(QIODevice::ReadOnly)) {
            const QJsonDocument mdoc = QJsonDocument::fromJson(mfile.readAll());
            mfile.close();
            if (mdoc.isObject()) {
                const QJsonArray versions = mdoc.object().value(QStringLiteral("versions")).toArray();
                for (const auto &v : versions) {
                    if (v.toObject().value(QStringLiteral("id")).toString() == versionId) {
                        jsonUrl = v.toObject().value(QStringLiteral("url")).toString();
                        break;
                    }
                }
            }
        }
    }

    if (!jsonUrl.isEmpty()) {
        fetchVersionJson(versionId, jsonUrl);
    } else {
        // Fetch manifest first to locate the version URL
        LOG_INFO(QStringLiteral("Fetching manifest to locate version %1...").arg(versionId));
        QNetworkRequest req((QUrl(Constants::VERSION_MANIFEST_URL)));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(15000);

        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, versionId, manifestPath]() {
            reply->deleteLater();
            if (m_cancelled) return;

            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || (statusCode != 0 && statusCode >= 400)) {
                abortAll(QStringLiteral("Failed to fetch version manifest (HTTP %1): %2")
                             .arg(statusCode).arg(reply->errorString()));
                return;
            }

            const QByteArray data = reply->readAll();
            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
            if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
                abortAll(QStringLiteral("Failed to parse version manifest: %1").arg(parseErr.errorString()));
                return;
            }

            // Cache manifest to effective game directory cache
            QFile mfile(manifestPath);
            if (mfile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                mfile.write(data);
                mfile.close();
            }

            QString resolvedUrl;
            const QJsonArray versions = doc.object().value(QStringLiteral("versions")).toArray();
            for (const auto &v : versions) {
                if (v.toObject().value(QStringLiteral("id")).toString() == versionId) {
                    resolvedUrl = v.toObject().value(QStringLiteral("url")).toString();
                    break;
                }
            }

            if (resolvedUrl.isEmpty()) {
                abortAll(QStringLiteral("Version %1 not found in Mojang manifest.").arg(versionId));
                return;
            }

            fetchVersionJson(versionId, resolvedUrl);
        });
    }
}

void Downloader::fetchVersionJson(const QString &versionId, const QString &jsonUrl) {
    LOG_INFO(QStringLiteral("Fetching version JSON from %1").arg(jsonUrl));
    emit statusChanged(QStringLiteral("Downloading version definition for %1...").arg(versionId));

    QNetworkRequest req;
    req.setUrl(QUrl(jsonUrl));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, versionId]() {
        reply->deleteLater();
        if (m_cancelled) return;

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || (statusCode != 0 && statusCode >= 400)) {
            abortAll(QStringLiteral("Failed to download version JSON for %1 (HTTP %2): %3")
                         .arg(versionId)
                         .arg(statusCode)
                         .arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();

        // Validate JSON before saving to avoid corrupted file lockout
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            abortAll(QStringLiteral("Corrupted or invalid version JSON received for %1: %2")
                         .arg(versionId, parseError.errorString()));
            return;
        }

        const QString versionDir = QDir(m_minecraftDir).filePath(QStringLiteral("versions/%1").arg(versionId));
        Utils::ensureDirectoryExists(versionDir);
        const QString jsonPath = QDir(versionDir).filePath(QStringLiteral("%1.json").arg(versionId));
        const QString tmpPath = jsonPath + QStringLiteral(".tmp");

        QFile tmpFile(tmpPath);
        if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            abortAll(QStringLiteral("Failed to open temporary file for writing: %1").arg(tmpPath));
            return;
        }
        tmpFile.write(data);
        tmpFile.close();

        // Atomically rename .tmp to .json upon valid JSON syntax
        QFile::remove(jsonPath);
        if (!QFile::rename(tmpPath, jsonPath)) {
            if (QFile::copy(tmpPath, jsonPath)) {
                QFile::remove(tmpPath);
            } else {
                QFile::remove(tmpPath);
                abortAll(QStringLiteral("Failed to finalize version JSON file: %1").arg(jsonPath));
                return;
            }
        }

        // If an ancestor version JSON was fetched during hierarchy resolution, resume target version
        if (versionId != m_targetVersionId && !m_targetVersionId.isEmpty()) {
            LOG_INFO(QStringLiteral("Ancestor version JSON %1 downloaded; resuming hierarchy for %2")
                         .arg(versionId, m_targetVersionId));
            startDownloadChain(m_targetVersionId);
        } else {
            handleVersionJsonDownloaded(versionId, data);
        }
    });
}

void Downloader::handleVersionJsonDownloaded(const QString &versionId, const QByteArray &data) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        abortAll(QStringLiteral("Corrupted version JSON for %1").arg(versionId));
        return;
    }

    QJsonObject mergedObj;
    if (!processVersionHierarchy(versionId, mergedObj)) {
        return;
    }

    prepareDownloadQueue(versionId, mergedObj);
}

bool Downloader::processVersionHierarchy(const QString &versionId, QJsonObject &mergedObj) {
    // Build the inheritance chain from leaf (target) to root parent:
    // [leaf, parent, grandparent, ...]
    QVector<QJsonObject> chain;
    QSet<QString> visited;
    QString currentId = versionId;

    while (!currentId.isEmpty()) {
        if (visited.contains(currentId)) {
            abortAll(QStringLiteral("Cyclic inheritance detected in version %1").arg(currentId));
            return false;
        }
        visited.insert(currentId);

        const QString currentJsonPath = QDir(m_minecraftDir).filePath(
            QStringLiteral("versions/%1/%1.json").arg(currentId));

        if (!QFile::exists(currentJsonPath)) {
            emit statusChanged(QStringLiteral("Parent version %1 required, resolving...").arg(currentId));
            LOG_INFO(QStringLiteral("Inheritance requires version %1 which is missing locally, fetching...").arg(currentId));
            startDownloadChain(currentId);
            return false;
        }

        QFile pFile(currentJsonPath);
        if (!pFile.open(QIODevice::ReadOnly)) {
            abortAll(QStringLiteral("Failed to open version file: %1").arg(currentJsonPath));
            return false;
        }

        const QByteArray fData = pFile.readAll();
        pFile.close();

        QJsonParseError pError;
        const QJsonDocument pDoc = QJsonDocument::fromJson(fData, &pError);
        if (pError.error != QJsonParseError::NoError || !pDoc.isObject()) {
            LOG_WARN(QStringLiteral("Corrupted version JSON for %1 detected, removing and re-fetching...").arg(currentId));
            QFile::remove(currentJsonPath);
            startDownloadChain(currentId);
            return false;
        }

        const QJsonObject curObj = pDoc.object();
        chain.append(curObj);

        if (curObj.contains(QStringLiteral("inheritsFrom"))) {
            currentId = curObj.value(QStringLiteral("inheritsFrom")).toString().trimmed();
        } else {
            currentId.clear();
        }
    }

    if (chain.isEmpty()) {
        abortAll(QStringLiteral("No version metadata available for %1").arg(versionId));
        return false;
    }

    // Merge chain from root (oldest ancestor: chain.last()) to leaf (target version: chain.first())
    mergedObj = chain.last();
    for (int i = chain.size() - 2; i >= 0; --i) {
        const QJsonObject child = chain[i];

        // 1. Scalar fields: child overrides parent
        for (const QString &key : {QStringLiteral("mainClass"), QStringLiteral("assets"),
                                   QStringLiteral("type"), QStringLiteral("releaseTime"),
                                   QStringLiteral("id")}) {
            if (child.contains(key) && !child[key].isNull()) {
                mergedObj[key] = child[key];
            }
        }

        // 2. Client download: if child specifies downloads, child wins; otherwise inherit parent
        if (child.contains(QStringLiteral("downloads")) && child[QStringLiteral("downloads")].isObject()) {
            const QJsonObject childDls = child.value(QStringLiteral("downloads")).toObject();
            if (childDls.contains(QStringLiteral("client"))) {
                mergedObj[QStringLiteral("downloads")] = childDls;
            }
        }

        // 3. Asset index: child overrides if present
        if (child.contains(QStringLiteral("assetIndex")) && child[QStringLiteral("assetIndex")].isObject()) {
            mergedObj[QStringLiteral("assetIndex")] = child[QStringLiteral("assetIndex")];
        }
        if (child.contains(QStringLiteral("assets")) && !child[QStringLiteral("assets")].isNull()) {
            mergedObj[QStringLiteral("assets")] = child[QStringLiteral("assets")];
        }

        // 4. minecraftArguments: concatenate strings
        if (child.contains(QStringLiteral("minecraftArguments"))) {
            const QString childArgs = child.value(QStringLiteral("minecraftArguments")).toString().trimmed();
            const QString parentArgs = mergedObj.value(QStringLiteral("minecraftArguments")).toString().trimmed();
            if (!childArgs.isEmpty()) {
                mergedObj[QStringLiteral("minecraftArguments")] = parentArgs.isEmpty()
                    ? childArgs
                    : QStringLiteral("%1 %2").arg(parentArgs, childArgs);
            }
        }

        // 5. arguments { game, jvm }: concatenate arrays
        if (child.contains(QStringLiteral("arguments")) && child[QStringLiteral("arguments")].isObject()) {
            QJsonObject mergedArgs = mergedObj.value(QStringLiteral("arguments")).toObject();
            const QJsonObject childArgs = child.value(QStringLiteral("arguments")).toObject();

            for (const QString &key : {QStringLiteral("game"), QStringLiteral("jvm")}) {
                QJsonArray items = mergedArgs.value(key).toArray();
                const QJsonArray add = childArgs.value(key).toArray();
                for (const auto &item : add) {
                    items.append(item);
                }
                mergedArgs[key] = items;
            }
            mergedObj[QStringLiteral("arguments")] = mergedArgs;
        }

        // 6. Libraries: parent libraries first, then child libraries
        QJsonArray mergedLibs = mergedObj.value(QStringLiteral("libraries")).toArray();
        const QJsonArray childLibs = child.value(QStringLiteral("libraries")).toArray();
        for (const auto &cl : childLibs) {
            mergedLibs.append(cl);
        }
        mergedObj[QStringLiteral("libraries")] = mergedLibs;
    }

    return true;
}

bool Downloader::isRuleAllowed(const QJsonArray &rules) const {
    if (rules.isEmpty()) {
        return true;
    }

    const QString osName = Utils::currentOSName(); // "windows", "linux", "osx"
    const QString arch = Utils::currentArch();     // "x64", "x86", etc.

    bool allowed = false;
    bool matchedAny = false;

    for (const auto &rVal : rules) {
        if (!rVal.isObject()) continue;
        const QJsonObject rule = rVal.toObject();

        bool ruleMatches = true;

        // OS check
        if (rule.contains(QStringLiteral("os")) && rule[QStringLiteral("os")].isObject()) {
            const QJsonObject osObj = rule[QStringLiteral("os")].toObject();
            if (osObj.contains(QStringLiteral("name"))) {
                if (osObj.value(QStringLiteral("name")).toString() != osName) {
                    ruleMatches = false;
                }
            }
            if (ruleMatches && osObj.contains(QStringLiteral("arch"))) {
                const QString ruleArch = osObj.value(QStringLiteral("arch")).toString();
                const bool wantX64 = (ruleArch == QStringLiteral("x64") || ruleArch == QStringLiteral("amd64") || ruleArch == QStringLiteral("x86_64"));
                const bool isX64 = (arch == QStringLiteral("x64"));
                if (wantX64 != isX64) {
                    ruleMatches = false;
                }
            }
        }

        // Features check
        if (ruleMatches && rule.contains(QStringLiteral("features")) && rule[QStringLiteral("features")].isObject()) {
            const QJsonObject features = rule[QStringLiteral("features")].toObject();
            for (auto it = features.begin(); it != features.end(); ++it) {
                const QString feat = it.key();
                const bool val = it.value().toBool(false);
                if (feat == QStringLiteral("has_custom_resolution")) {
                    if (val != true) ruleMatches = false;
                } else if (feat == QStringLiteral("is_demo_user")) {
                    if (val != false) ruleMatches = false;
                } else {
                    if (val) ruleMatches = false;
                }
            }
        }

        if (ruleMatches) {
            matchedAny = true;
            allowed = (rule.value(QStringLiteral("action")).toString() == QStringLiteral("allow"));
        }
    }

    return matchedAny ? allowed : false;
}

QString Downloader::parseMavenPath(const QString &coordinates) const {
    const QStringList parts = coordinates.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }

    const QString group = QString(parts[0]).replace(QLatin1Char('.'), QLatin1Char('/'));
    const QString artifact = parts[1];
    const QString version = parts[2];
    QString classifier;
    QString ext = QStringLiteral("jar");

    if (parts.size() >= 4) {
        const QString rest = parts[3];
        const int atIdx = rest.indexOf(QLatin1Char('@'));
        if (atIdx != -1) {
            classifier = rest.left(atIdx);
            ext = rest.mid(atIdx + 1);
        } else {
            classifier = rest;
        }
    }

    if (!classifier.isEmpty()) {
        return QStringLiteral("%1/%2/%3/%2-%3-%4.%5").arg(group, artifact, version, classifier, ext);
    }
    return QStringLiteral("%1/%2/%3/%2-%3.%4").arg(group, artifact, version, ext);
}

void Downloader::prepareDownloadQueue(const QString &versionId, const QJsonObject &versionData) {
    emit statusChanged(QStringLiteral("Analyzing dependencies and assets for %1...").arg(versionId));

    // 1. Client JAR
    const QString clientJarPath = QDir(m_minecraftDir).filePath(
        QStringLiteral("versions/%1/%1.jar").arg(versionId));

    if (versionData.contains(QStringLiteral("downloads")) &&
        versionData[QStringLiteral("downloads")].isObject()) {
        const QJsonObject dlObj = versionData[QStringLiteral("downloads")].toObject();
        if (dlObj.contains(QStringLiteral("client")) && dlObj[QStringLiteral("client")].isObject()) {
            const QJsonObject client = dlObj[QStringLiteral("client")].toObject();
            const QString url = client.value(QStringLiteral("url")).toString();
            const qint64 size = client.value(QStringLiteral("size")).toInteger(0);
            const QString sha1 = client.value(QStringLiteral("sha1")).toString();

            bool hasValidJar = Utils::isFileValid(clientJarPath, size, sha1);
            if (!hasValidJar && versionData.contains(QStringLiteral("inheritsFrom"))) {
                const QString parentId = versionData.value(QStringLiteral("inheritsFrom")).toString();
                const QString parentJar = QDir(m_minecraftDir).filePath(
                    QStringLiteral("versions/%1/%1.jar").arg(parentId));
                if (Utils::isFileValid(parentJar, size, sha1)) {
                    Utils::ensureDirectoryExists(QFileInfo(clientJarPath).absolutePath());
                    QFile::copy(parentJar, clientJarPath);
                    hasValidJar = true;
                    LOG_INFO(QStringLiteral("Reused parent client JAR from %1 for %2").arg(parentId, versionId));
                }
            }

            if (!url.isEmpty() && !hasValidJar) {
                DownloadTask task;
                task.url = QUrl(url);
                task.destinationPath = clientJarPath;
                task.expectedSize = size;
                task.expectedSha1 = sha1;
                task.description = QStringLiteral("Client JAR");
                m_queue.append(task);
                m_totalBytes += size;
            }
        }
    }

    // 2. Libraries
    const QString osName = Utils::currentOSName(); // "windows", "linux", "osx"
    const QString librariesDir = QDir(m_minecraftDir).filePath(QStringLiteral("libraries"));

    if (versionData.contains(QStringLiteral("libraries")) && versionData[QStringLiteral("libraries")].isArray()) {
        const QJsonArray libs = versionData[QStringLiteral("libraries")].toArray();
        for (const auto &lVal : libs) {
            if (!lVal.isObject()) continue;
            const QJsonObject lib = lVal.toObject();

            // Check rules
            if (lib.contains(QStringLiteral("rules")) && !isRuleAllowed(lib[QStringLiteral("rules")].toArray())) {
                continue;
            }

            // Check standard artifact
            if (lib.contains(QStringLiteral("downloads")) && lib[QStringLiteral("downloads")].isObject()) {
                const QJsonObject dls = lib[QStringLiteral("downloads")].toObject();

                // Standard jar artifact
                if (dls.contains(QStringLiteral("artifact")) && dls[QStringLiteral("artifact")].isObject()) {
                    const QJsonObject art = dls[QStringLiteral("artifact")].toObject();
                    const QString relPath = art.value(QStringLiteral("path")).toString();
                    const QString url = art.value(QStringLiteral("url")).toString();
                    const qint64 size = art.value(QStringLiteral("size")).toInteger(0);
                    const QString sha1 = art.value(QStringLiteral("sha1")).toString();

                    if (!relPath.isEmpty() && !url.isEmpty()) {
                        const QString dest = QDir(librariesDir).filePath(relPath);
                        if (!Utils::isFileValid(dest, size, sha1)) {
                            DownloadTask task;
                            task.url = QUrl(url);
                            task.destinationPath = dest;
                            task.expectedSize = size;
                            task.expectedSha1 = sha1;
                            task.description = QStringLiteral("Library: %1").arg(QFileInfo(relPath).fileName());
                            m_queue.append(task);
                            m_totalBytes += size;
                        }
                    }
                }

                // Native classifiers
                if (lib.contains(QStringLiteral("natives")) && lib[QStringLiteral("natives")].isObject() &&
                    dls.contains(QStringLiteral("classifiers")) && dls[QStringLiteral("classifiers")].isObject()) {
                    const QJsonObject natives = lib[QStringLiteral("natives")].toObject();
                    if (natives.contains(osName)) {
                        QString classifier = natives.value(osName).toString();
                        classifier.replace(QStringLiteral("${arch}"),
                                           (Utils::currentArch() == QStringLiteral("x64")) ? QStringLiteral("64") : QStringLiteral("32"));

                        const QJsonObject classifiers = dls[QStringLiteral("classifiers")].toObject();
                        if (classifiers.contains(classifier)) {
                            const QJsonObject natObj = classifiers.value(classifier).toObject();
                            const QString relPath = natObj.value(QStringLiteral("path")).toString();
                            const QString url = natObj.value(QStringLiteral("url")).toString();
                            const qint64 size = natObj.value(QStringLiteral("size")).toInteger(0);
                            const QString sha1 = natObj.value(QStringLiteral("sha1")).toString();

                            if (!relPath.isEmpty() && !url.isEmpty()) {
                                const QString dest = QDir(librariesDir).filePath(relPath);
                                m_nativeJarsToExtract.append(dest);
                                if (!Utils::isFileValid(dest, size, sha1)) {
                                    DownloadTask task;
                                    task.url = QUrl(url);
                                    task.destinationPath = dest;
                                    task.expectedSize = size;
                                    task.expectedSha1 = sha1;
                                    task.isNative = true;
                                    task.description = QStringLiteral("Natives: %1").arg(QFileInfo(relPath).fileName());
                                    m_queue.append(task);
                                    m_totalBytes += size;
                                }
                            }
                        }
                    }
                }
            } else if (lib.contains(QStringLiteral("name"))) {
                // Legacy maven coordinates format (< 1.13)
                const QString name = lib.value(QStringLiteral("name")).toString();
                const QString relPath = parseMavenPath(name);
                if (!relPath.isEmpty()) {
                    const QString baseUrl = lib.value(QStringLiteral("url")).toString(QStringLiteral("https://libraries.minecraft.net/"));
                    const QString url = QStringLiteral("%1/%2").arg(baseUrl.endsWith(QLatin1Char('/')) ? baseUrl.left(baseUrl.length() - 1) : baseUrl, relPath);
                    const QString dest = QDir(librariesDir).filePath(relPath);

                    if (!Utils::isFileValid(dest, -1, QString())) {
                        DownloadTask task;
                        task.url = QUrl(url);
                        task.destinationPath = dest;
                        task.description = QStringLiteral("Maven library: %1").arg(QFileInfo(relPath).fileName());
                        m_queue.append(task);
                    }
                }
            }
        }
    }

    // 3. Asset Index and Objects
    if (versionData.contains(QStringLiteral("assetIndex")) && versionData[QStringLiteral("assetIndex")].isObject()) {
        const QJsonObject assetIndexObj = versionData[QStringLiteral("assetIndex")].toObject();
        const QString indexId = assetIndexObj.value(QStringLiteral("id")).toString();
        const QString indexUrl = assetIndexObj.value(QStringLiteral("url")).toString();
        const qint64 indexSize = assetIndexObj.value(QStringLiteral("size")).toInteger(0);
        const QString indexSha1 = assetIndexObj.value(QStringLiteral("sha1")).toString();

        const QString indexDest = QDir(m_minecraftDir).filePath(
            QStringLiteral("assets/indexes/%1.json").arg(indexId));

        if (!indexUrl.isEmpty()) {
            if (!Utils::isFileValid(indexDest, indexSize, indexSha1)) {
                // Fetch asset index first, then collect asset objects
                emit statusChanged(QStringLiteral("Downloading asset index for %1...").arg(indexId));
                QNetworkRequest aReq;
                aReq.setUrl(QUrl(indexUrl));
                aReq.setHeader(QNetworkRequest::UserAgentHeader,
                               QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
                aReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                aReq.setTransferTimeout(15000);

                QNetworkReply *aReply = m_nam->get(aReq);
                connect(aReply, &QNetworkReply::finished, this, [this, aReply, indexDest, indexId, versionId]() {
                    aReply->deleteLater();
                    if (m_cancelled) return;

                    const int statusCode = aReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (aReply->error() != QNetworkReply::NoError || (statusCode != 0 && statusCode >= 400)) {
                        LOG_WARN(QStringLiteral("Failed to download asset index %1 (HTTP %2): %3")
                                     .arg(indexId).arg(statusCode).arg(aReply->errorString()));
                    } else {
                        const QByteArray aData = aReply->readAll();
                        Utils::ensureDirectoryExists(QFileInfo(indexDest).absolutePath());
                        QFile aFile(indexDest);
                        if (aFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            aFile.write(aData);
                            aFile.close();
                        }

                        // Parse asset objects from downloaded index
                        const QJsonDocument aDoc = QJsonDocument::fromJson(aData);
                        if (aDoc.isObject() && aDoc.object().contains(QStringLiteral("objects"))) {
                            const QJsonObject objects = aDoc.object().value(QStringLiteral("objects")).toObject();
                            const QString assetsObjectsDir = QDir(m_minecraftDir).filePath(QStringLiteral("assets/objects"));

                            for (auto it = objects.begin(); it != objects.end(); ++it) {
                                const QJsonObject obj = it.value().toObject();
                                const QString hash = obj.value(QStringLiteral("hash")).toString();
                                const qint64 size = obj.value(QStringLiteral("size")).toInteger(0);

                                if (hash.length() >= 2) {
                                    const QString subDir = hash.left(2);
                                    const QString dest = QStringLiteral("%1/%2/%3").arg(assetsObjectsDir, subDir, hash);

                                    if (!Utils::isFileValid(dest, size, hash)) {
                                        DownloadTask task;
                                        task.url = QUrl(QStringLiteral("%1%2/%3").arg(Constants::RESOURCES_BASE_URL, subDir, hash));
                                        task.destinationPath = dest;
                                        task.expectedSize = size;
                                        task.expectedSha1 = hash;
                                        task.description = QStringLiteral("Asset: %1").arg(it.key());
                                        m_queue.append(task);
                                        m_totalBytes += size;
                                    }
                                }
                            }
                        }
                    }

                    // Now start queue execution
                    m_totalTasks = m_queue.size();
                    m_completedTasks = 0;
                    LOG_INFO(QStringLiteral("Total download queue prepared: %1 files (%2 bytes)").arg(m_totalTasks).arg(m_totalBytes));
                    if (m_queue.isEmpty()) {
                        finishInstallation();
                    } else {
                        processQueue();
                    }
                });
                return;
            } else {
                // Read local asset index
                QFile aFile(indexDest);
                if (aFile.open(QIODevice::ReadOnly)) {
                    const QJsonDocument aDoc = QJsonDocument::fromJson(aFile.readAll());
                    aFile.close();

                    if (aDoc.isObject() && aDoc.object().contains(QStringLiteral("objects"))) {
                        const QJsonObject objects = aDoc.object().value(QStringLiteral("objects")).toObject();
                        const QString assetsObjectsDir = QDir(m_minecraftDir).filePath(QStringLiteral("assets/objects"));

                        for (auto it = objects.begin(); it != objects.end(); ++it) {
                            const QJsonObject obj = it.value().toObject();
                            const QString hash = obj.value(QStringLiteral("hash")).toString();
                            const qint64 size = obj.value(QStringLiteral("size")).toInteger(0);

                            if (hash.length() >= 2) {
                                const QString subDir = hash.left(2);
                                const QString dest = QStringLiteral("%1/%2/%3").arg(assetsObjectsDir, subDir, hash);

                                if (!Utils::isFileValid(dest, size, hash)) {
                                    DownloadTask task;
                                    task.url = QUrl(QStringLiteral("%1%2/%3").arg(Constants::RESOURCES_BASE_URL, subDir, hash));
                                    task.destinationPath = dest;
                                    task.expectedSize = size;
                                    task.expectedSha1 = hash;
                                    task.description = QStringLiteral("Asset: %1").arg(it.key());
                                    m_queue.append(task);
                                    m_totalBytes += size;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    m_totalTasks = m_queue.size();
    m_completedTasks = 0;
    LOG_INFO(QStringLiteral("Total download queue prepared: %1 files (%2 bytes)").arg(m_totalTasks).arg(m_totalBytes));

    if (m_queue.isEmpty()) {
        finishInstallation();
    } else {
        processQueue();
    }
}

void Downloader::processQueue() {
    if (m_cancelled) return;

    while (m_activeDownloads.size() < MAX_CONCURRENT_DOWNLOADS && !m_queue.isEmpty()) {
        const DownloadTask task = m_queue.takeFirst();

        Utils::ensureDirectoryExists(QFileInfo(task.destinationPath).absolutePath());
        const QString tempPath = task.destinationPath + QStringLiteral(".tmp");

        QNetworkRequest req(task.url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(15000);

        QNetworkReply *reply = m_nam->get(req);

        ActiveDownload active;
        active.task = task;
        active.tempFilePath = tempPath;
        active.bytesReceived = 0;
        m_activeDownloads.insert(reply, active);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onReplyFinished(reply);
        });
        connect(reply, &QNetworkReply::downloadProgress, this, &Downloader::onReplyProgress);
    }

    if (m_activeDownloads.isEmpty() && m_queue.isEmpty()) {
        finishInstallation();
    }
}

void Downloader::onReplyProgress(qint64 bytesReceived, qint64 /*bytesTotal*/) {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    ActiveDownload &active = m_activeDownloads[reply];
    const qint64 delta = bytesReceived - active.bytesReceived;
    if (delta > 0) {
        active.bytesReceived = bytesReceived;
        m_completedBytes += delta;

        const qint64 totalReport = (m_totalBytes > 0) ? m_totalBytes : 1;
        emit downloadProgress(m_completedBytes, totalReport);
        const int percent = qBound(0, static_cast<int>((m_completedBytes * 100) / totalReport), 100);
        emit progressPercent(percent);
    }
}

void Downloader::onReplyFinished(QNetworkReply *reply) {
    if (!m_activeDownloads.contains(reply)) {
        reply->deleteLater();
        return;
    }

    ActiveDownload active = m_activeDownloads.take(reply);
    reply->deleteLater();

    if (m_cancelled) {
        QFile::remove(active.tempFilePath);
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (statusCode != 0 && statusCode >= 400)) {
        LOG_WARN(QStringLiteral("Download failed for %1 (HTTP %2): %3")
                     .arg(active.task.url.toString())
                     .arg(statusCode)
                     .arg(reply->errorString()));
        QFile::remove(active.tempFilePath);

        if (active.task.retryCount < MAX_RETRIES) {
            active.task.retryCount++;
            LOG_INFO(QStringLiteral("Retrying download (%1/%2): %3").arg(active.task.retryCount).arg(MAX_RETRIES).arg(active.task.url.toString()));
            m_queue.prepend(active.task);
        } else {
            LOG_ERROR(QStringLiteral("Permanent failure downloading %1").arg(active.task.url.toString()));
            // Client JAR or version files are critical
            if (active.task.destinationPath.endsWith(QStringLiteral(".jar")) &&
                active.task.destinationPath.contains(QStringLiteral("versions"))) {
                abortAll(QStringLiteral("Failed to download client JAR: %1").arg(reply->errorString()));
                return;
            }
        }
        processQueue();
        return;
    }

    // Write downloaded data
    const QByteArray data = reply->readAll();
    QFile file(active.tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(QStringLiteral("Failed to open temp file for writing: %1").arg(active.tempFilePath));
        processQueue();
        return;
    }
    file.write(data);
    file.close();

    // Verify SHA1 and size if specified
    bool valid = true;
    if (active.task.expectedSize > 0 && file.size() != active.task.expectedSize) {
        valid = false;
        LOG_WARN(QStringLiteral("Size mismatch for %1: expected %2, got %3")
                     .arg(active.task.destinationPath)
                     .arg(active.task.expectedSize)
                     .arg(file.size()));
    }
    if (valid && !active.task.expectedSha1.isEmpty()) {
        if (!Utils::verifyFileSha1(active.tempFilePath, active.task.expectedSha1)) {
            valid = false;
            LOG_WARN(QStringLiteral("SHA1 mismatch for %1").arg(active.task.destinationPath));
        }
    }

    if (!valid) {
        QFile::remove(active.tempFilePath);
        if (active.task.retryCount < MAX_RETRIES) {
            active.task.retryCount++;
            m_queue.prepend(active.task);
        } else {
            LOG_ERROR(QStringLiteral("Checksum failure after retries for %1").arg(active.task.destinationPath));
        }
        processQueue();
        return;
    }

    // Atomic move temp file to destination
    QFile::remove(active.task.destinationPath);
    if (!QFile::rename(active.tempFilePath, active.task.destinationPath)) {
        // Fallback copy if rename fails across partitions
        QFile::copy(active.tempFilePath, active.task.destinationPath);
        QFile::remove(active.tempFilePath);
    }

    m_completedTasks++;
    const int percent = (m_totalTasks > 0) ? (m_completedTasks * 100 / m_totalTasks) : 100;
    emit statusChanged(QStringLiteral("Downloading (%1/%2): %3")
                           .arg(m_completedTasks)
                           .arg(m_totalTasks)
                           .arg(QFileInfo(active.task.destinationPath).fileName()));
    emit progressPercent(percent);

    processQueue();
}

void Downloader::finishInstallation() {
    if (m_cancelled) return;

    emit statusChanged(QStringLiteral("Extracting native libraries..."));
    extractAllNatives();

    m_busy = false;
    LOG_INFO(QStringLiteral("Installation completed successfully for version %1").arg(m_targetVersionId));
    emit statusChanged(QStringLiteral("Installation of %1 completed!").arg(m_targetVersionId));
    emit progressPercent(100);
    emit finished();
}

void Downloader::extractAllNatives() {
    const QString nativesDir = QDir(m_minecraftDir).filePath(
        QStringLiteral("versions/%1/natives").arg(m_targetVersionId));
    Utils::ensureDirectoryExists(nativesDir);

    for (const QString &jarPath : m_nativeJarsToExtract) {
        if (QFile::exists(jarPath)) {
            extractNativeJar(jarPath, nativesDir);
        }
    }
}

bool Downloader::extractNativeJar(const QString &jarPath, const QString &targetDir) {
    LOG_INFO(QStringLiteral("Extracting natives from %1 to %2").arg(jarPath, targetDir));

    // Try using unzip first: unzip -q -o -j <jar> "*.so" "*.dll" "*.dylib" -d <targetDir>
    QProcess unzipProc;
    QStringList args;
    args << QStringLiteral("-q")
         << QStringLiteral("-o")
         << QStringLiteral("-j")
         << jarPath
         << QStringLiteral("*.so")
         << QStringLiteral("*.dll")
         << QStringLiteral("*.dylib")
         << QStringLiteral("-d")
         << targetDir;

    unzipProc.start(QStringLiteral("unzip"), args);
    if (unzipProc.waitForFinished(10000) && unzipProc.exitCode() == 0) {
        return true;
    }

    // Fallback: tar -xf <jar> -C <targetDir> --wildcards "*.so" "*.dll" "*.dylib"
    QProcess tarProc;
    QStringList tarArgs;
    tarArgs << QStringLiteral("-xf")
            << jarPath
            << QStringLiteral("-C")
            << targetDir
            << QStringLiteral("--wildcards")
            << QStringLiteral("*.so")
            << QStringLiteral("*.dll")
            << QStringLiteral("*.dylib");

    tarProc.start(QStringLiteral("tar"), tarArgs);
    if (tarProc.waitForFinished(10000) && tarProc.exitCode() == 0) {
        return true;
    }

    return false;
}

void Downloader::abortAll(const QString &reason) {
    LOG_ERROR(QStringLiteral("Installation aborted: %1").arg(reason));
    m_busy = false;
    m_queue.clear();
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.key()) {
            it.key()->abort();
            it.key()->deleteLater();
        }
        QFile::remove(it.value().tempFilePath);
    }
    m_activeDownloads.clear();

    emit statusChanged(QStringLiteral("Error: %1").arg(reason));
    emit error(reason);
}
