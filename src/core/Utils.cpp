#include "Utils.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QSysInfo>
#include <algorithm>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <QFile>
#include <QTextStream>
#endif

namespace Utils {

quint64 getTotalSystemMemoryMb() {
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<quint64>(memStatus.ullTotalPhys / (1024 * 1024));
    }
#elif defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/meminfo"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(QStringLiteral("MemTotal:"))) {
                static const QRegularExpression re(QStringLiteral(R"(MemTotal:\s+(\d+)\s+kB)"));
                auto match = re.match(line);
                if (match.hasMatch()) {
                    quint64 totalKb = match.captured(1).toULongLong();
                    return totalKb / 1024;
                }
            }
        }
    }
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0) {
        return static_cast<quint64>(pages) * static_cast<quint64>(pageSize) / (1024 * 1024);
    }
#elif defined(Q_OS_MAC)
    int mib[2];
    int64_t memBytes = 0;
    size_t length = sizeof(memBytes);
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    if (sysctl(mib, 2, &memBytes, &length, nullptr, 0) == 0) {
        return static_cast<quint64>(memBytes / (1024 * 1024));
    }
#endif
    return 4096; // safe fallback
}

quint64 getOptimalRamMb() {
    quint64 totalMb = getTotalSystemMemoryMb();
    quint64 mb = 2048;

    if (totalMb <= 4096) {
        mb = std::max<quint64>(1024, totalMb / 4);
    } else if (totalMb <= 8192) {
        mb = totalMb / 3;
    } else {
        mb = std::min<quint64>(totalMb / 2, 8192);
    }

    // Align to 128 MB blocks for cleaner values
    mb = (mb / 128) * 128;
    if (mb < 1024) {
        mb = 1024;
    }
    return mb;
}

qint64 getAvailableDiskSpaceMb(const QString &path) {
    QStorageInfo storage(path);
    if (!storage.isValid() || !storage.isReady()) {
        QDir dir(path);
        while (!dir.exists() && !dir.isRoot()) {
            dir.cdUp();
        }
        storage.setPath(dir.absolutePath());
    }

    if (storage.isValid() && storage.isReady()) {
        return storage.bytesAvailable() / (1024 * 1024);
    }
    return -1;
}

QPair<qint64, bool> checkDiskSpace(const QString &path, qint64 requiredMb) {
    qint64 freeMb = getAvailableDiskSpaceMb(path);
    if (freeMb < 0) {
        // Could not detect, don't block
        return qMakePair(static_cast<qint64>(0), true);
    }
    return qMakePair(freeMb, freeMb >= requiredMb);
}

OperatingSystem currentOS() {
#if defined(Q_OS_WIN)
    return OperatingSystem::Windows;
#elif defined(Q_OS_MAC)
    return OperatingSystem::MacOS;
#elif defined(Q_OS_LINUX)
    return OperatingSystem::Linux;
#else
    return OperatingSystem::Unknown;
#endif
}

QString currentOSName() {
    switch (currentOS()) {
    case OperatingSystem::Windows:
        return QStringLiteral("windows");
    case OperatingSystem::MacOS:
        return QStringLiteral("osx");
    case OperatingSystem::Linux:
        return QStringLiteral("linux");
    default:
        return QStringLiteral("unknown");
    }
}

QString currentArch() {
    QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch == QStringLiteral("x86_64") || arch == QStringLiteral("amd64") || arch == QStringLiteral("x64")) {
        return QStringLiteral("x64");
    } else if (arch == QStringLiteral("i386") || arch == QStringLiteral("i686") || arch == QStringLiteral("x86")) {
        return QStringLiteral("x86");
    } else if (arch.startsWith(QStringLiteral("arm64")) || arch == QStringLiteral("aarch64")) {
        return QStringLiteral("arm64");
    } else if (arch.startsWith(QStringLiteral("arm"))) {
        return QStringLiteral("arm");
    }
    return arch;
}

bool ensureDirectoryExists(const QString &path) {
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(QStringLiteral("."));
}

bool openInFileManager(const QString &path) {
    ensureDirectoryExists(path);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

qint64 getDirectorySize(const QString &path) {
    qint64 size = 0;
    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fileInfo : list) {
        if (fileInfo.isDir()) {
            size += getDirectorySize(fileInfo.absoluteFilePath());
        } else {
            size += fileInfo.size();
        }
    }
    return size;
}

bool isValidNickname(const QString &nickname) {
    static const QRegularExpression re(QStringLiteral("^[a-zA-Z0-9_]{3,16}$"));
    return re.match(nickname).hasMatch();
}

QString generateOfflinePlayerUuid(const QString &username) {
    // Standard Minecraft offline UUID calculation (RFC 4122 v3 MD5 with zero namespace)
    QByteArray data;
    data.fill(0, 16); // 16 bytes of zero namespace
    data.append("OfflinePlayer:");
    data.append(username.toUtf8());

    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    if (hash.size() != 16) {
        return QString();
    }

    auto *bytes = reinterpret_cast<unsigned char *>(hash.data());
    // Version 3 (MD5 based)
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x30);
    // RFC 4122 variant (10xxxxxx)
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);

    return QStringLiteral("%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16")
        .arg(bytes[0], 2, 16, QLatin1Char('0'))
        .arg(bytes[1], 2, 16, QLatin1Char('0'))
        .arg(bytes[2], 2, 16, QLatin1Char('0'))
        .arg(bytes[3], 2, 16, QLatin1Char('0'))
        .arg(bytes[4], 2, 16, QLatin1Char('0'))
        .arg(bytes[5], 2, 16, QLatin1Char('0'))
        .arg(bytes[6], 2, 16, QLatin1Char('0'))
        .arg(bytes[7], 2, 16, QLatin1Char('0'))
        .arg(bytes[8], 2, 16, QLatin1Char('0'))
        .arg(bytes[9], 2, 16, QLatin1Char('0'))
        .arg(bytes[10], 2, 16, QLatin1Char('0'))
        .arg(bytes[11], 2, 16, QLatin1Char('0'))
        .arg(bytes[12], 2, 16, QLatin1Char('0'))
        .arg(bytes[13], 2, 16, QLatin1Char('0'))
        .arg(bytes[14], 2, 16, QLatin1Char('0'))
        .arg(bytes[15], 2, 16, QLatin1Char('0'))
        .toLower();
}

QList<int> parseVersion(const QString &versionStr) {
    QList<int> result;
    QStringList parts = versionStr.trimmed().split(QRegularExpression(QStringLiteral(R"([.\-_])")));
    static const QRegularExpression numRe(QStringLiteral(R"(^(\d+))"));

    for (int i = 0; i < parts.size(); ++i) {
        auto match = numRe.match(parts[i]);
        if (match.hasMatch()) {
            int val = match.captured(1).toInt();
            // Match LightLauncher normalization for versions like 0.5 vs 0.43
            if (i == 1 && parts.size() > 0 && parts[0] == QStringLiteral("0") && match.captured(1).length() == 1 && val < 10) {
                val *= 10;
            }
            result.append(val);
        }
    }
    if (result.isEmpty()) {
        result.append(0);
    }
    return result;
}

int compareVersions(const QString &v1, const QString &v2) {
    QList<int> t1 = parseVersion(v1);
    QList<int> t2 = parseVersion(v2);
    int len = std::max(t1.size(), t2.size());
    for (int i = 0; i < len; ++i) {
        int x = (i < t1.size()) ? t1[i] : 0;
        int y = (i < t2.size()) ? t2[i] : 0;
        if (x != y) {
            return (x > y) ? 1 : -1;
        }
    }
    return 0;
}

bool isVersionNewer(const QString &v1, const QString &v2) {
    return compareVersions(v1, v2) > 0;
}

QString computeSha1(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    char buf[65536];
    qint64 bytesRead = 0;
    while ((bytesRead = file.read(buf, sizeof(buf))) > 0) {
        hash.addData(QByteArrayView(buf, bytesRead));
    }
    return QString::fromUtf8(hash.result().toHex()).toLower();
}

bool verifyFileSha1(const QString &filePath, const QString &expectedSha1) {
    if (expectedSha1.trimmed().isEmpty()) {
        return true;
    }
    QString actual = computeSha1(filePath);
    return actual.compare(expectedSha1.trimmed(), Qt::CaseInsensitive) == 0;
}

bool isFileValid(const QString &filePath, qint64 expectedSize, const QString &expectedSha1) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    if (expectedSize >= 0 && info.size() != expectedSize) {
        return false;
    }
    if (!expectedSha1.isEmpty()) {
        return verifyFileSha1(filePath, expectedSha1);
    }
    return true;
}

bool isCacheValid(const QString &filePath, qint64 maxAgeSeconds) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    return info.lastModified().secsTo(QDateTime::currentDateTime()) < maxAgeSeconds;
}

} // namespace Utils
