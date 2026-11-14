cmake_minimum_required(VERSION 3.16)
project(CLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

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
)

set(UI_SOURCES
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ConsoleWindow.h
    src/ui/ConsoleWindow.cpp
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
)

if(WIN32)
    target_link_libraries(CLauncher PRIVATE user32 shell32)
endif()
