#pragma once

#include <QString>
#include <QStringList>
#include <QPair>
#include <QList>
#include <QtGlobal>

namespace Utils {

    enum class OperatingSystem {
        Windows,
        Linux,
        MacOS,
        Unknown
    };

    // System info & RAM
    quint64 getTotalSystemMemoryMb();
    inline quint64 getSystemTotalRamMb() { return getTotalSystemMemoryMb(); }
    quint64 getOptimalRamMb();

    // Disk space
    QPair<qint64, bool> checkDiskSpace(const QString &path, qint64 requiredMb = 2048);
    qint64 getAvailableDiskSpaceMb(const QString &path);

    // OS & Arch
    OperatingSystem currentOS();
    QString currentOSName(); // "windows", "linux", "osx" (matches Mojang rules)
    QString currentArch();   // "x64", "x86", "arm64", etc.

    // Filesystem helpers
    bool ensureDirectoryExists(const QString &path);
    bool openInFileManager(const QString &path);
    qint64 getDirectorySize(const QString &path);

    // Validation
    bool isValidNickname(const QString &nickname);

    // Offline player UUID (MD5 version 3 RFC 4122 for "OfflinePlayer:<name>")
    QString generateOfflinePlayerUuid(const QString &username);

    // Version parsing & comparison
    QList<int> parseVersion(const QString &versionStr);
    int compareVersions(const QString &v1, const QString &v2);
    bool isVersionNewer(const QString &v1, const QString &v2);

    // Checksums & cache
    QString computeSha1(const QString &filePath);
    bool verifyFileSha1(const QString &filePath, const QString &expectedSha1);
    bool isFileValid(const QString &filePath, qint64 expectedSize = -1, const QString &expectedSha1 = QString());
    bool isCacheValid(const QString &filePath, qint64 maxAgeSeconds = 3600);

} // namespace Utils
