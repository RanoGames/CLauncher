QT += core gui widgets network

CONFIG += c++17

TEMPLATE = app
TARGET = CLauncher

INCLUDEPATH += src

# Core Module
HEADERS += \
    src/core/Constants.h \
    src/core/Logger.h \
    src/core/Utils.h \
    src/core/Settings.h \
    src/core/PlaytimeTracker.h

SOURCES += \
    src/core/Logger.cpp \
    src/core/Utils.cpp \
    src/core/Settings.cpp \
    src/core/PlaytimeTracker.cpp

# Minecraft Engine Module
HEADERS += \
    src/mc/VersionManifest.h \
    src/mc/Downloader.h \
    src/mc/JavaManager.h \
    src/mc/GameLauncher.h

SOURCES += \
    src/mc/VersionManifest.cpp \
    src/mc/Downloader.cpp \
    src/mc/JavaManager.cpp \
    src/mc/GameLauncher.cpp

# UI Module & Application Entry
HEADERS += \
    src/ui/MainWindow.h \
    src/ui/SettingsDialog.h \
    src/ui/ConsoleWindow.h

SOURCES += \
    src/main.cpp \
    src/ui/MainWindow.cpp \
    src/ui/SettingsDialog.cpp \
    src/ui/ConsoleWindow.cpp

# Resources
RESOURCES += \
    resources/resources.qrc

win32 {
    LIBS += -luser32 -lshell32
}
