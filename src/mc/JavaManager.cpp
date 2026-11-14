#include "mc/JavaManager.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

int JavaManager::parseJavaMajorVersion(const QString &javaExecutable) {
    if (!QFile::exists(javaExecutable)) {
        return 0;
    }

    QProcess proc;
    proc.setProgram(javaExecutable);
    proc.setArguments(QStringList{QStringLiteral("-version")});
    proc.setProcessChannelMode(QProcess::MergedChannels);

    proc.start();
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        return 0;
    }

    const QString output = QString::fromUtf8(proc.readAll());
    // Pattern matches:
    // 'openjdk version "17.0.2"' -> 17
    // 'java version "1.8.0_401"' -> 8
    // 'java version "21.0.1"'    -> 21
    static const QRegularExpression re(QStringLiteral(R"(version\s+"(?:1\.)?(\d+))"));
    const QRegularExpressionMatch match = re.match(output);

    if (match.hasMatch()) {
        bool ok = false;
        const int major = match.captured(1).toInt(&ok);
        if (ok && major > 0) {
            return major;
        }
    }

    return 0;
}

int JavaManager::getRequiredJavaMajor(const QString &mcVersion) {
    const QString lower = mcVersion.toLower().trimmed();

    // Check snapshots like "24w14a"
    static const QRegularExpression snapRe(QStringLiteral(R"(^(\d{2})w\d+[a-z])"));
    const auto snapMatch = snapRe.match(lower);
    if (snapMatch.hasMatch()) {
        const int year = snapMatch.captured(1).toInt();
        if (year >= 24) return 21;
        if (year >= 22) return 17;
        if (year == 21) return 16;
        return 8;
    }

    // Standard version pattern "1.X" or "1.X.Y"
    static const QRegularExpression verRe(QStringLiteral(R"(1\.(\d+)(?:\.(\d+))?)"));
    const auto verMatch = verRe.match(mcVersion);
    if (!verMatch.hasMatch()) {
        return 8; // Default fallback for vintage versions
    }

    const int minor = verMatch.captured(1).toInt();
    const int patch = verMatch.captured(2).isEmpty() ? 0 : verMatch.captured(2).toInt();

    if (minor >= 21 || (minor == 20 && patch >= 5)) {
        return 21;
    }
    if (minor >= 18) {
        return 17;
    }
    if (minor == 17) {
        return 16;
    }
    return 8;
}

QList<JavaInfo> JavaManager::findInstalledJavas(const QString &customMinecraftDir) {
    QStringList foundPaths;
    QSet<QString> seenCanonicals;

    const QString mcDir = customMinecraftDir.isEmpty() ? Constants::defaultMinecraftDirectory() : customMinecraftDir;
    QStringList searchDirs;

#if defined(Q_OS_WIN)
    const QString progFiles = QProcessEnvironment::systemEnvironment().value(QStringLiteral("PROGRAMFILES"), QStringLiteral("C:\\Program Files"));
    const QString progFilesX86 = QProcessEnvironment::systemEnvironment().value(QStringLiteral("PROGRAMFILES(X86)"), QStringLiteral("C:\\Program Files (x86)"));
    const QString localAppData = QProcessEnvironment::systemEnvironment().value(QStringLiteral("LOCALAPPDATA"));

    const QStringList vendors = {
        QStringLiteral("Java"),
        QStringLiteral("Eclipse Adoptium"),
        QStringLiteral("BellSoft"),
        QStringLiteral("Microsoft"),
        QStringLiteral("Zulu"),
        QStringLiteral("Semeru")
    };

    for (const QString &base : {progFiles, progFilesX86}) {
        for (const QString &vendor : vendors) {
            searchDirs.append(QDir(base).filePath(vendor));
        }
    }

    if (!localAppData.isEmpty()) {
        searchDirs.append(QDir(localAppData).filePath(QStringLiteral("Programs/Eclipse Adoptium")));
    }
#else
    searchDirs << QStringLiteral("/usr/lib/jvm")
               << QStringLiteral("/usr/java")
               << QStringLiteral("/opt/java")
               << QStringLiteral("/usr/local/java")
               << QStringLiteral("/Library/Java/JavaVirtualMachines")
               << QDir::homePath() + QStringLiteral("/.sdkman/candidates/java");
#endif

    // Mojang runtime directory inside Minecraft directory
    searchDirs.append(QDir(mcDir).filePath(QStringLiteral("runtime")));

    // Search inside designated directories
    for (const QString &dirPath : searchDirs) {
        if (!QDir(dirPath).exists()) continue;

        QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString fileName = it.fileName().toLower();
#if defined(Q_OS_WIN)
            if (fileName == QStringLiteral("javaw.exe") || fileName == QStringLiteral("java.exe")) {
#else
            if (fileName == QStringLiteral("java")) {
#endif
                const QString canonical = QFileInfo(it.filePath()).canonicalFilePath();
                if (!canonical.isEmpty() && !seenCanonicals.contains(canonical)) {
                    seenCanonicals.insert(canonical);
                    foundPaths.append(canonical);
                }
            }
        }
    }

    // System PATH search
    const QString sysJava = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!sysJava.isEmpty()) {
        const QString canonical = QFileInfo(sysJava).canonicalFilePath();
        if (!canonical.isEmpty() && !seenCanonicals.contains(canonical)) {
            seenCanonicals.insert(canonical);
            foundPaths.append(canonical);
        }
    }
#if defined(Q_OS_WIN)
    const QString sysJavaw = QStandardPaths::findExecutable(QStringLiteral("javaw"));
    if (!sysJavaw.isEmpty()) {
        const QString canonical = QFileInfo(sysJavaw).canonicalFilePath();
        if (!canonical.isEmpty() && !seenCanonicals.contains(canonical)) {
            seenCanonicals.insert(canonical);
            foundPaths.append(canonical);
        }
    }
#endif

    // Sort paths so javaw is prioritized over java on Windows
    std::sort(foundPaths.begin(), foundPaths.end(), [](const QString &a, const QString &b) {
        const bool aIsW = a.toLower().endsWith(QStringLiteral("javaw.exe"));
        const bool bIsW = b.toLower().endsWith(QStringLiteral("javaw.exe"));
        if (aIsW != bIsW) return aIsW;
        return a < b;
    });

    QList<JavaInfo> result;
    QSet<QString> seenParentDirs;

    for (const QString &path : foundPaths) {
        const QString parentDir = QFileInfo(path).absolutePath().toLower();
        if (seenParentDirs.contains(parentDir)) {
            continue;
        }

        const int major = parseJavaMajorVersion(path);
        if (major > 0) {
            seenParentDirs.insert(parentDir);
            JavaInfo info;
            info.path = path;
            info.majorVersion = major;
            info.versionString = QStringLiteral("Java %1").arg(major);
            info.name = QStringLiteral("Java %1 (%2)").arg(QString::number(major), path);
            result.append(info);
        }
    }

    // Sort by major version descending (21, 17, 16, 8...)
    std::sort(result.begin(), result.end(), [](const JavaInfo &a, const JavaInfo &b) {
        return a.majorVersion > b.majorVersion;
    });

    return result;
}

JavaCheckResult JavaManager::selectBestJava(const QString &mcVersion,
                                           const QString &userJavaPath,
                                           bool autoDetect,
                                           const QString &customMinecraftDir) {
    JavaCheckResult result;
    const int reqMajor = getRequiredJavaMajor(mcVersion);

    // If auto-detection is disabled and user provided a specific path
    if (!autoDetect && !userJavaPath.trimmed().isEmpty()) {
        if (!QFile::exists(userJavaPath)) {
            result.javaPath = QString();
            result.warning = QStringLiteral("Specified Java path does not exist.");
            result.isCompatible = false;
            return result;
        }

        const int actualMajor = parseJavaMajorVersion(userJavaPath);
        result.javaPath = userJavaPath;
        result.isCompatible = true;

        if (actualMajor > 0 && actualMajor != reqMajor) {
            if (reqMajor == 8 && actualMajor > 8) {
                result.warning = QStringLiteral("Minecraft %1 requires Java 8, but Java %2 is selected. The game may crash!")
                                     .arg(mcVersion, QString::number(actualMajor));
            } else if (reqMajor > actualMajor) {
                result.warning = QStringLiteral("Minecraft %1 requires at least Java %2, but Java %3 is selected!")
                                     .arg(mcVersion, QString::number(reqMajor), QString::number(actualMajor));
            }
        }
        return result;
    }

    // Auto-detection mode
    const QList<JavaInfo> installed = findInstalledJavas(customMinecraftDir);

    if (installed.isEmpty()) {
        // Fallback to system java executable in PATH
        const QString sysJava = QStandardPaths::findExecutable(QStringLiteral("java"));
        if (!sysJava.isEmpty() && QFile::exists(sysJava)) {
            const int actualMajor = parseJavaMajorVersion(sysJava);
            result.javaPath = sysJava;
            result.isCompatible = true;
            if (actualMajor > 0 && actualMajor < reqMajor) {
                result.warning = QStringLiteral("Minecraft %1 requires Java %2, but system Java is version %3.")
                                     .arg(mcVersion, QString::number(reqMajor), QString::number(actualMajor));
            }
            return result;
        }

        result.javaPath = QString();
        result.warning = QStringLiteral("Java was not found on your system! Please install Java to launch Minecraft.");
        result.isCompatible = false;
        return result;
    }

    // 1. Exact version match
    for (const auto &j : installed) {
        if (j.majorVersion == reqMajor) {
            result.javaPath = j.path;
            result.isCompatible = true;
            result.warning = QString();
            return result;
        }
    }

    // 2. Compatibility fallback: Java 17+ can often run Minecraft requiring 16 or 17
    if (reqMajor == 16 || reqMajor == 17) {
        // Find smallest major >= reqMajor
        for (auto it = installed.rbegin(); it != installed.rend(); ++it) {
            if (it->majorVersion >= reqMajor) {
                result.javaPath = it->path;
                result.isCompatible = true;
                result.warning = QString();
                return result;
            }
        }
    }

    // 3. Fallback to newest installed with a clear warning
    const JavaInfo &fallback = installed.first();
    result.javaPath = fallback.path;
    result.isCompatible = true;
    result.warning = QStringLiteral("Minecraft %1 recommends Java %2, but only Java %3 was found. Game may crash!")
                         .arg(mcVersion, QString::number(reqMajor), QString::number(fallback.majorVersion));
    return result;
}

QString JavaManager::getMojangRuntimeName(int majorVersion) {
    if (majorVersion <= 8) {
        return QStringLiteral("jre-legacy");
    }
    if (majorVersion == 16) {
        return QStringLiteral("java-runtime-alpha");
    }
    if (majorVersion >= 17 && majorVersion <= 20) {
        return QStringLiteral("java-runtime-gamma");
    }
    return QStringLiteral("java-runtime-delta");
}

QString JavaManager::getRuntimeNameForVersion(const QString &mcVersion) {
    const int major = getRequiredJavaMajor(mcVersion);
    return getMojangRuntimeName(major);
}

QString JavaManager::getRuntimeJavaPath(const QString &runtimeName, const QString &customMinecraftDir) {
    const QString mcDir = customMinecraftDir.isEmpty() ? Constants::defaultMinecraftDirectory() : customMinecraftDir;
    const QString rtDir = QDir(mcDir).filePath(QStringLiteral("runtime/%1").arg(runtimeName));

    if (!QDir(rtDir).exists()) {
        return QString();
    }

    QDirIterator it(rtDir, QDir::Files, QDirIterator::Subdirectories);
    QString fallbackJava;

    while (it.hasNext()) {
        it.next();
        const QString fileName = it.fileName().toLower();
#if defined(Q_OS_WIN)
        if (fileName == QStringLiteral("javaw.exe")) {
            return it.filePath();
        }
        if (fileName == QStringLiteral("java.exe")) {
            fallbackJava = it.filePath();
        }
#else
        if (fileName == QStringLiteral("java")) {
            return it.filePath();
        }
#endif
    }

    return fallbackJava;
}

bool JavaManager::isRuntimeInstalled(const QString &runtimeName, const QString &customMinecraftDir) {
    return !getRuntimeJavaPath(runtimeName, customMinecraftDir).isEmpty();
}
