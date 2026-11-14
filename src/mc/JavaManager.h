#pragma once

#include <QString>
#include <QStringList>
#include <QList>

struct JavaInfo {
    QString path;
    int majorVersion = 0;
    QString versionString;
    QString name;
};

struct JavaCheckResult {
    QString javaPath;
    QString warning;
    bool isCompatible = true;
};

class JavaManager {
public:
    JavaManager() = delete;

    // Parse major version from executable by running `-version`
    static int parseJavaMajorVersion(const QString &javaExecutable);

    // Get required major version for a given Minecraft version string
    static int getRequiredJavaMajor(const QString &mcVersion);

    // Locate all installed Java environments
    static QList<JavaInfo> findInstalledJavas(const QString &customMinecraftDir = QString());

    // Select the best Java runtime for Minecraft version
    static JavaCheckResult selectBestJava(const QString &mcVersion,
                                         const QString &userJavaPath = QString(),
                                         bool autoDetect = true,
                                         const QString &customMinecraftDir = QString());

    // Mojang runtime names
    static QString getMojangRuntimeName(int majorVersion);
    static QString getRuntimeNameForVersion(const QString &mcVersion);
    static QString getRuntimeJavaPath(const QString &runtimeName, const QString &customMinecraftDir = QString());
    static bool isRuntimeInstalled(const QString &runtimeName, const QString &customMinecraftDir = QString());
};
