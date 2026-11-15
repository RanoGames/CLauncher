# CLauncher

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-6.x-green.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**CLauncher** is a modern, high-performance, native C++20 / Qt 6 Minecraft launcher. Designed as a native rewrite and evolution of LightLauncher, it eliminates Python runtime dependencies, delivering near-instantaneous startup times, minimal memory consumption, and rock-solid stability across Linux and Windows.

---

## ✨ Features

- **🚀 Native Performance & Lightweight Footprint**:
  - Pure C++23 with Qt 6 Widgets and Network stack.
  - Zero Python runtime or interpreter overhead.
  - Rapid UI responsiveness, instant cold start, and optimized memory usage.

- **🎮 Full Minecraft Lifecycle Support**:
  - **Mojang Manifest V2 Integration**: Asynchronous query and caching of official releases, snapshots, and legacy beta/alpha versions.
  - **Automated Dependency & Asset Downloader**: Multi-threaded parallel asset, library, and `client.jar` downloads with hash integrity verification and OS/architecture native rule evaluation.
  - **Modular Launch Engine**: Classpath assembly with multi-version compatibility checks, native library extraction, customizable JVM arguments (`-Xmx`, `-Xms`, custom GC/flags), and real-time game process supervision.

- **🧩 Native Mod Management & Modrinth Browser**:
  - **Mod Manager**: Full-featured manager for `.minecraft/mods/`. Displays mod display name, version, loader badge, and file size by inspecting in-memory ZIP central directories (`fabric.mod.json`, `quilt.mod.json`, `META-INF/mods.toml`, `META-INF/neoforge.mods.toml`, `mcmod.info`).
  - **Interactive Toggling**: Toggle mods on the fly by renaming between `.jar` and `.jar.disabled`, with bulk "Enable All" / "Disable All" and individual deletion.
  - **Modrinth v2 Browser**: Search and browse thousands of mods directly from Modrinth with loader filters (Fabric, Forge, NeoForge, Quilt) and version matching. Includes 1-click download directly into the active mods directory with live download progress.

- **🧶 Native Modloader Installation**:
  - **Fabric Installer**: Connects to the official Fabric Meta API (`meta.fabricmc.net`), resolves stable/compatible loader versions for any Minecraft release, and automatically installs atomic version JSON profiles.

- **☕ Smart Java Detection & Adoptium Downloader**:
  - **Local Runtime Scanner**: Detects local JRE/JDK installations (Java 8, Java 17, Java 21+) on Linux (`/usr/lib/jvm`, `PATH`) and Windows (Registry, Program Files) with version compatibility matching.
  - **Adoptium Temurin Downloader**: Automatically queries Adoptium v3 API to download, extract, and configure portable JRE environments into `runtime/` when no compatible system Java is detected.

- **💬 Native Discord Rich Presence (RPC)**:
  - High-performance, pure C++ IPC via `QLocalSocket` connecting to Discord daemon sockets (`$XDG_RUNTIME_DIR/discord-ipc-0..9` on Linux, `\\.\pipe\discord-ipc-0..9` on Windows).
  - Displays real-time playing status, current Minecraft version, and session elapsed time with zero third-party library overhead.

- **👤 Skin & Avatar System**:
  - Fetches player skins from Ely.by and Mojang with local disk caching.
  - Dynamically extracts 8x8 face rects, composites the outer helmet/hat layer, and scales antialiased avatar previews displayed directly alongside the nickname selector.
  - Offline default Steve fallback when disconnected or unskinned.

- **🩺 Intelligent Crash Log Analyzer**:
  - Automatically parses game exit codes, `crash-reports/*.txt`, and `latest.log`.
  - Diagnoses OutOfMemoryError, outdated GPU drivers (OpenGL/GLFW error 65542), incompatible Java runtime versions, missing mod dependencies, and Mixin errors.
  - Displays structured diagnoses with clear root causes, actionable troubleshooting recommendations, and error snippet previews.

- **🎨 Modern Dark Interface & Optimizations**:
  - Sleek glassmorphism theme with custom QSS stylesheet.
  - Nickname regex validation (`^[a-zA-Z0-9_]{3,16}$`) and quick nickname selector.
  - Cached background rendering eliminating unnecessary CPU repainting.
  - Action dock with quick access pills for Mods, Catalog, Settings, Console, and `.minecraft` directory.
  - Auto-updating launcher check via GitHub Releases API.

- **📊 Integrated Console & Playtime Tracking**:
  - Live stdout/stderr log viewer capturing game output and launcher debug events.
  - Accurate tracking of total game playtime stored safely using atomic `QSaveFile`.

---

## 📁 Project Structure

```
CLauncher/
├── CLauncher.pro              # qmake6 project specification (Qt6 + ZLIB)
├── CMakeLists.txt             # Modern CMake build configuration
├── BUILD.md                   # Detailed cross-platform compilation guide
├── README.md                  # Project overview and documentation
├── resources/
│   ├── resources.qrc          # Qt Resource bundle
│   ├── style.qss              # Custom dark theme stylesheet
│   ├── background.png         # Launcher header/background art
│   ├── folder.png             # UI icon for folder access
│   ├── github.png             # UI icon for GitHub link
│   └── mclovin.ico            # Application icon
└── src/
    ├── main.cpp               # Application entry point and logging setup
    ├── core/                  # Core utilities and diagnostic subsystems
    │   ├── Constants.h        # File paths, URLs, version defaults
    │   ├── Settings.h/.cpp    # Atomic JSON-backed configuration manager
    │   ├── Logger.h/.cpp      # Thread-safe logging subsystem
    │   ├── Utils.h/.cpp       # System information, offline UUID, and path helpers
    │   ├── PlaytimeTracker.h/.cpp # Playtime tracking subsystem with atomic persistence
    │   ├── CrashAnalyzer.h/.cpp   # Automated crash diagnosis & log analyzer
    │   └── Updater.h/.cpp     # GitHub Releases update checker & downloader
    ├── mc/                    # Minecraft game engine and network services
    │   ├── VersionManifest.h/.cpp # Mojang version metadata parser & fetcher
    │   ├── Downloader.h/.cpp  # Parallel asset and library download engine
    │   ├── JavaManager.h/.cpp # Local JRE scanner and version compatibility
    │   ├── GameLauncher.h/.cpp# Classpath builder and process supervisor
    │   ├── DiscordRpc.h/.cpp  # Native Discord Rich Presence IPC client
    │   ├── SkinManager.h/.cpp # Ely.by/Mojang skin fetcher & avatar renderer
    │   ├── ModloaderInstaller.h/.cpp # Fabric loader installer via Fabric Meta API
    │   ├── JavaDownloader.h/.cpp # Adoptium Temurin portable JRE downloader
    │   ├── ModManager.h/.cpp  # Local mods inspector, toggler, and manager
    │   └── ModBrowser.h/.cpp  # Modrinth v2 API client and mod downloader
    └── ui/                    # Graphical user interface
        ├── MainWindow.h/.cpp  # Main launcher dashboard with action dock
        ├── SettingsDialog.h/.cpp # Settings dialog (RAM, Java, game directory, theme)
        ├── ConsoleWindow.h/.cpp  # Real-time game log viewer
        ├── ModManagerDialog.h/.cpp # Installed mods manager dialog
        ├── ModBrowserDialog.h/.cpp # Modrinth mod catalog browser dialog
        └── SkinPreviewWidget.h/.cpp # Live 2D player avatar preview widget
```

---

## 🛠️ Quick Start

### Linux Build (Quick)

Ensure `qt6-base-dev`, `zlib`, and build tools are installed:

```bash
# Ubuntu / Debian
sudo apt install -y build-essential qt6-base-dev qt6-base-dev-tools zlib1g-dev

# Arch Linux
sudo pacman -Syu base-devel qt6-base zlib

# Build with qmake6
qmake6 CLauncher.pro
make -j$(nproc)

# Run
./CLauncher
```

### Windows Native Build (Quick)

1. Open `CLauncher.pro` or `CMakeLists.txt` in **Qt Creator** with the **Qt 6 MSVC 2022** or **MinGW 64-bit** kit.
2. Hit **Build & Run** (`Ctrl + R`).
3. For standalone distribution, run `windeployqt --release CLauncher.exe` to package all required Qt runtime DLLs.

> 📖 For comprehensive instructions, MSVC command prompt workflows, MinGW steps, and cross-compilation guides, see **[BUILD.md](BUILD.md)**.

---

## 🎮 Usage Guide

1. **Launch the Application**: Run `./CLauncher` (Linux) or `CLauncher.exe` (Windows).
2. **Set your Nickname**: Enter your player username in the top input field.
3. **Select Version**: Choose your desired Minecraft version from the dropdown menu (e.g., `1.20.4`).
4. **Configure Settings (Optional)**:
   - Click the gear icon to adjust allocated RAM (default: 2048 MB).
   - Configure a specific Java version if not using auto-detection.
5. **Click Play**: CLauncher will automatically download necessary assets, libraries, and launch Minecraft. The Console Window can be opened at any time to inspect live game logs.

---

## 📜 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
