cmake_minimum_required(VERSION 3.16)
project(CLauncher VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

find_package(ZLIB QUIET)
if(NOT TARGET ZLIB::ZLIB)
    include(FetchContent)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
    )
    FetchContent_MakeAvailable(zlib)
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
        target_include_directories(zlibstatic INTERFACE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
        target_include_directories(zlib INTERFACE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    endif()
endif()

set(CORE_SOURCES
    src/core/Constants.h
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/Utils.h
    src/core/Utils.cpp
    src/core/Settings.h
    src/core/Settings.cpp
    src/core/PlaytimeTracker.h
    src/core/PlaytimeTracker.cpp
    src/core/CrashAnalyzer.h
    src/core/CrashAnalyzer.cpp
    src/core/Updater.h
    src/core/Updater.cpp
    src/core/Account.h
    src/core/AccountManager.h
    src/core/AccountManager.cpp
)

set(MC_SOURCES
    src/mc/VersionManifest.h
    src/mc/VersionManifest.cpp
    src/mc/Downloader.h
    src/mc/Downloader.cpp
    src/mc/JavaManager.h
    src/mc/JavaManager.cpp
    src/mc/GameLauncher.h
    src/mc/GameLauncher.cpp
    src/mc/ModManager.h
    src/mc/ModManager.cpp
    src/mc/ModBrowser.h
    src/mc/ModBrowser.cpp
    src/mc/DiscordRpc.h
    src/mc/DiscordRpc.cpp
    src/mc/SkinManager.h
    src/mc/SkinManager.cpp
    src/mc/ModloaderInstaller.h
    src/mc/ModloaderInstaller.cpp
    src/mc/JavaDownloader.h
    src/mc/JavaDownloader.cpp
)

set(UI_SOURCES
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ConsoleWindow.h
    src/ui/ConsoleWindow.cpp
    src/ui/ModManagerDialog.h
    src/ui/ModManagerDialog.cpp
    src/ui/ModBrowserDialog.h
    src/ui/ModBrowserDialog.cpp
    src/ui/SkinPreviewWidget.h
    src/ui/SkinPreviewWidget.cpp
    src/ui/AccountManagerDialog.h
    src/ui/AccountManagerDialog.cpp
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(CLauncher
    ${CORE_SOURCES}
    ${MC_SOURCES}
    ${UI_SOURCES}
    ${RESOURCES}
)

target_include_directories(CLauncher PRIVATE src)

target_link_libraries(CLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ZLIB::ZLIB
)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(CLauncher PRIVATE user32 shell32)
endif()
