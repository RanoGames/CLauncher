#pragma once

#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QProcessEnvironment>

namespace Constants {

    // Launcher metadata
    inline const QString LAUNCHER_NAME = QStringLiteral("CLauncher");
    inline const QString LAUNCHER_VERSION = QStringLiteral("1.0.1");
    inline const QString DISCORD_CLIENT_ID = QStringLiteral("1420709615468810403");

    // URLs
    inline const QString VERSION_MANIFEST_URL = QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json");
    inline const QString GITHUB_API_URL = QStringLiteral("https://api.github.com/repos/RanoGames/CLauncher/releases/latest");
    inline const QString GITHUB_REPO_URL = QStringLiteral("https://github.com/RanoGames/CLauncher");
    inline const QString RESOURCES_BASE_URL = QStringLiteral("https://resources.download.minecraft.net/");
    inline const QString ELY_BY_SKIN_URL = QStringLiteral("http://skinsystem.ely.by/textures/");
    inline const QString MODRINTH_API_URL = QStringLiteral("https://api.modrinth.com/v2");

    // Default Settings Values
    inline constexpr int DEFAULT_MEMORY_MB = 2048;
    inline constexpr int MIN_MEMORY_MB = 1024;
    inline constexpr int MAX_MEMORY_MB = 65536;
    inline constexpr int DEFAULT_RESOLUTION_WIDTH = 1280;
    inline constexpr int DEFAULT_RESOLUTION_HEIGHT = 720;
    inline const QString DEFAULT_UI_THEME = QStringLiteral("modern");
    inline const QString DEFAULT_LANGUAGE = QStringLiteral("auto");

    // Path resolution functions
    inline QString defaultMinecraftDirectory() {
#if defined(Q_OS_WIN)
        QString appData = QProcessEnvironment::systemEnvironment().value(QStringLiteral("APPDATA"));
        if (!appData.isEmpty()) {
            return QDir::cleanPath(appData + QStringLiteral("/.minecraft"));
        }
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/AppData/Roaming/.minecraft"));
#elif defined(Q_OS_MAC)
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/Library/Application Support/minecraft"));
#else
        return QDir::cleanPath(QDir::homePath() + QStringLiteral("/.minecraft"));
#endif
    }

    inline QString defaultSettingsPath() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/settings.json"));
    }

    inline QString defaultCacheDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/cache"));
    }

    inline QString defaultLogPath() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/launcher.log"));
    }

    inline QString defaultPlaytimePath() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/playtime.json"));
    }

    inline QString defaultVersionsDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/versions"));
    }

    inline QString defaultLibrariesDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/libraries"));
    }

    inline QString defaultAssetsDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/assets"));
    }

    inline QString defaultModsDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/mods"));
    }

    inline QString defaultCrashReportsDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/crash-reports"));
    }

    inline QString defaultLogsDirectory() {
        return QDir::cleanPath(defaultMinecraftDirectory() + QStringLiteral("/logs"));
    }

} // namespace Constants
