QT += core gui widgets network

CONFIG += c++23

TEMPLATE = app
TARGET = CLauncher

INCLUDEPATH += src

# Core Module
HEADERS += \
    src/core/Constants.h \
    src/core/Logger.h \
    src/core/Utils.h \
    src/core/Settings.h \
    src/core/PlaytimeTracker.h \
    src/core/CrashAnalyzer.h \
    src/core/Updater.h \
    src/core/Account.h \
    src/core/AccountManager.h

SOURCES += \
    src/core/Logger.cpp \
    src/core/Utils.cpp \
    src/core/Settings.cpp \
    src/core/PlaytimeTracker.cpp \
    src/core/CrashAnalyzer.cpp \
    src/core/Updater.cpp \
    src/core/AccountManager.cpp

# Minecraft Engine Module
HEADERS += \
    src/mc/VersionManifest.h \
    src/mc/Downloader.h \
    src/mc/JavaManager.h \
    src/mc/GameLauncher.h \
    src/mc/ModManager.h \
    src/mc/ModBrowser.h \
    src/mc/DiscordRpc.h \
    src/mc/SkinManager.h \
    src/mc/ModloaderInstaller.h \
    src/mc/JavaDownloader.h

SOURCES += \
    src/mc/VersionManifest.cpp \
    src/mc/Downloader.cpp \
    src/mc/JavaManager.cpp \
    src/mc/GameLauncher.cpp \
    src/mc/ModManager.cpp \
    src/mc/ModBrowser.cpp \
    src/mc/DiscordRpc.cpp \
    src/mc/SkinManager.cpp \
    src/mc/ModloaderInstaller.cpp \
    src/mc/JavaDownloader.cpp

# UI Module & Application Entry
HEADERS += \
    src/ui/MainWindow.h \
    src/ui/SettingsDialog.h \
    src/ui/ConsoleWindow.h \
    src/ui/ModManagerDialog.h \
    src/ui/ModBrowserDialog.h \
    src/ui/SkinPreviewWidget.h \
    src/ui/AccountManagerDialog.h

SOURCES += \
    src/main.cpp \
    src/ui/MainWindow.cpp \
    src/ui/SettingsDialog.cpp \
    src/ui/ConsoleWindow.cpp \
    src/ui/ModManagerDialog.cpp \
    src/ui/ModBrowserDialog.cpp \
    src/ui/SkinPreviewWidget.cpp \
    src/ui/AccountManagerDialog.cpp

# Resources
RESOURCES += \
    resources/resources.qrc

unix {
    LIBS += -lz
}

win32 {
    DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN
    LIBS += -luser32 -lshell32
    win32-g++: LIBS += -lz
}
