#include "mc/JavaDownloader.h"
#include "mc/JavaManager.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QNetworkRequest>

JavaDownloader::JavaDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

JavaDownloader::~JavaDownloader() {
    cancel();
}

void JavaDownloader::cancel() {
    if (m_busy) {
        m_cancelled = true;
        if (m_currentReply) {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        if (m_tempFile) {
            m_tempFile->close();
            delete m_tempFile;
            m_tempFile = nullptr;
        }
        if (!m_tempFilePath.isEmpty()) {
            QFile::remove(m_tempFilePath);
        }
        m_busy = false;
        emit statusChanged(QStringLiteral("Java download cancelled."));
        emit error(QStringLiteral("Java download cancelled by user."));
    }
}

QString JavaDownloader::determineOs() const {
    const QString osName = Utils::currentOSName();
    if (osName == QStringLiteral("windows")) return QStringLiteral("windows");
    if (osName == QStringLiteral("osx")) return QStringLiteral("mac");
    return QStringLiteral("linux");
}

QString JavaDownloader::determineArch() const {
    const QString arch = Utils::currentArch();
    if (arch == QStringLiteral("arm64") || arch == QStringLiteral("aarch64")) {
        return QStringLiteral("aarch64");
    }
    if (arch == QStringLiteral("x86") || arch == QStringLiteral("i386") || arch == QStringLiteral("x32")) {
        return QStringLiteral("x32");
    }
    return QStringLiteral("x64");
}

void JavaDownloader::downloadJavaForMcVersion(const QString &mcVersion, const QString &customMinecraftDir) {
    const int requiredMajor = JavaManager::getRequiredJavaMajor(mcVersion);
    downloadJava(requiredMajor, customMinecraftDir);
}

void JavaDownloader::downloadJava(int majorVersion, const QString &customMinecraftDir) {
    if (m_busy) {
        emit error(QStringLiteral("Downloader is already running."));
        return;
    }

    if (majorVersion <= 0) {
        majorVersion = 17;
    }

    m_busy = true;
    m_cancelled = false;
    m_majorVersion = majorVersion;
    m_minecraftDir = customMinecraftDir.isEmpty() ? Constants::defaultMinecraftDirectory() : customMinecraftDir;

    const QString os = determineOs();
    const QString arch = determineArch();
    const QString ext = (os == QStringLiteral("windows")) ? QStringLiteral("zip") : QStringLiteral("tar.gz");

    const QString runtimeDir = QDir(m_minecraftDir).filePath(QStringLiteral("runtime"));
    Utils::ensureDirectoryExists(runtimeDir);

    m_targetDir = QDir(runtimeDir).filePath(QStringLiteral("adoptium-%1").arg(m_majorVersion));
    Utils::ensureDirectoryExists(m_targetDir);

    m_tempFilePath = QDir(runtimeDir).filePath(QStringLiteral("adoptium-%1-archive.%2").arg(m_majorVersion).arg(ext));
    QFile::remove(m_tempFilePath);

    m_tempFile = new QFile(m_tempFilePath, this);
    if (!m_tempFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to open temporary file for Java archive: %1").arg(m_tempFilePath);
        LOG_ERROR(err);
        emit error(err);
        delete m_tempFile;
        m_tempFile = nullptr;
        return;
    }

    const QString urlStr = QStringLiteral("https://api.adoptium.net/v3/binary/latest/%1/ga/%2/%3/jre/hotspot/normal/eclipse")
                               .arg(m_majorVersion)
                               .arg(os)
                               .arg(arch);

    LOG_INFO(QStringLiteral("Requesting Adoptium JRE %1 for %2-%3 from %4").arg(m_majorVersion).arg(os, arch, urlStr));
    emit statusChanged(QStringLiteral("Connecting to Adoptium API for Java %1...").arg(m_majorVersion));
    emit progressPercent(0);

    QNetworkRequest req((QUrl(urlStr)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(Constants::LAUNCHER_NAME, Constants::LAUNCHER_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);

    m_currentReply = m_nam->get(req);

    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentReply && m_tempFile && m_tempFile->isOpen()) {
            m_tempFile->write(m_currentReply->readAll());
        }
    });
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &JavaDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &JavaDownloader::onReplyFinished);
}

void JavaDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (m_cancelled) return;

    emit downloadProgress(bytesReceived, bytesTotal);
    if (bytesTotal > 0) {
        const int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        emit progressPercent(percent);
        const double mbRec = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
        const double mbTot = static_cast<double>(bytesTotal) / (1024.0 * 1024.0);
        emit statusChanged(QStringLiteral("Downloading Java %1: %2 MB / %3 MB (%4%)")
                               .arg(m_majorVersion)
                               .arg(mbRec, 0, 'f', 1)
                               .arg(mbTot, 0, 'f', 1)
                               .arg(percent));
    } else {
        const double mbRec = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
        emit statusChanged(QStringLiteral("Downloading Java %1: %2 MB...").arg(m_majorVersion).arg(mbRec, 0, 'f', 1));
    }
}

void JavaDownloader::onReplyFinished() {
    if (!m_currentReply) return;

    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    reply->deleteLater();

    if (m_tempFile) {
        if (reply->bytesAvailable() > 0 && m_tempFile->isOpen()) {
            m_tempFile->write(reply->readAll());
        }
        m_tempFile->flush();
        m_tempFile->close();
        delete m_tempFile;
        m_tempFile = nullptr;
    }

    if (m_cancelled) {
        QFile::remove(m_tempFilePath);
        m_busy = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(m_tempFilePath);
        m_busy = false;
        const QString err = QStringLiteral("Failed to download Java runtime: %1").arg(reply->errorString());
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    const qint64 fileSize = QFileInfo(m_tempFilePath).size();
    if (fileSize < 1000000) { // Should be at least 1MB
        QFile::remove(m_tempFilePath);
        m_busy = false;
        const QString err = QStringLiteral("Adoptium archive is too small or corrupted (%1 bytes)").arg(fileSize);
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    emit statusChanged(QStringLiteral("Extracting Java %1 runtime...").arg(m_majorVersion));
    LOG_INFO(QStringLiteral("Extracting %1 into %2").arg(m_tempFilePath, m_targetDir));

    const bool extractOk = extractArchive(m_tempFilePath, m_targetDir);
    QFile::remove(m_tempFilePath);

    if (!extractOk) {
        m_busy = false;
        const QString err = QStringLiteral("Failed to extract Java archive to %1").arg(m_targetDir);
        LOG_ERROR(err);
        emit error(err);
        return;
    }

    const QString javaExe = findJavaExecutable(m_targetDir);
    if (javaExe.isEmpty() || !QFile::exists(javaExe)) {
        m_busy = false;
        const QString err = QStringLiteral("Java executable not found in extracted runtime %1").arg(m_targetDir);
        LOG_ERROR(err);
        emit error(err);
        return;
    }

#if !defined(Q_OS_WIN)
    // Ensure executable permissions
    QFile::setPermissions(javaExe, QFile::permissions(javaExe) |
                                       QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther |
                                       QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
#endif

    m_busy = false;
    LOG_INFO(QStringLiteral("Java %1 runtime successfully installed at %2").arg(m_majorVersion).arg(javaExe));
    emit progressPercent(100);
    emit statusChanged(QStringLiteral("Java %1 installed successfully!").arg(m_majorVersion));
    emit installFinished(javaExe);
}

bool JavaDownloader::extractArchive(const QString &archivePath, const QString &targetDir) {
    QProcess proc;
    QStringList args;

#if defined(Q_OS_WIN)
    // Windows 10/11 includes bsdtar which handles .zip
    args << QStringLiteral("-xf") << archivePath << QStringLiteral("-C") << targetDir;
    proc.start(QStringLiteral("tar"), args);
    if (proc.waitForFinished(60000) && proc.exitCode() == 0) {
        return true;
    }

    // Fallback using PowerShell
    QStringList psArgs;
    psArgs << QStringLiteral("-NoProfile")
           << QStringLiteral("-Command")
           << QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                  .arg(archivePath, targetDir);
    QProcess psProc;
    psProc.start(QStringLiteral("powershell"), psArgs);
    if (psProc.waitForFinished(120000) && psProc.exitCode() == 0) {
        return true;
    }
    return false;
#else
    // Linux/macOS: tar -xzf <archive> -C <targetDir>
    args << QStringLiteral("-xzf") << archivePath << QStringLiteral("-C") << targetDir;
    proc.start(QStringLiteral("tar"), args);
    if (proc.waitForFinished(60000) && proc.exitCode() == 0) {
        return true;
    }

    // Fallback: unzip or tar without gzip flag if uncompressed
    QProcess fallbackProc;
    QStringList fbArgs;
    fbArgs << QStringLiteral("-xf") << archivePath << QStringLiteral("-C") << targetDir;
    fallbackProc.start(QStringLiteral("tar"), fbArgs);
    if (fallbackProc.waitForFinished(60000) && fallbackProc.exitCode() == 0) {
        return true;
    }
    return false;
#endif
}

QString JavaDownloader::findJavaExecutable(const QString &targetDir) const {
    QDirIterator it(targetDir, QDir::Files, QDirIterator::Subdirectories);
    QString fallback;

    while (it.hasNext()) {
        it.next();
        const QString fileName = it.fileName().toLower();
#if defined(Q_OS_WIN)
        if (fileName == QStringLiteral("javaw.exe")) {
            return it.filePath();
        }
        if (fileName == QStringLiteral("java.exe")) {
            fallback = it.filePath();
        }
#else
        if (fileName == QStringLiteral("java") && it.fileInfo().path().endsWith(QStringLiteral("/bin"))) {
            return it.filePath();
        }
#endif
    }

    return fallback;
}
