# CLauncher

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-6.x-green.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**CLauncher** is a modern, high-performance, native C++20 / Qt 6 Minecraft launcher. Designed as a native rewrite and evolution of LightLauncher, it eliminates Python runtime dependencies, delivering near-instantaneous startup times, minimal memory consumption, and rock-solid stability across Linux and Windows.

---

## ✨ Features

- **🚀 Native Performance & Lightweight Footprint**:
  - Pure C++20 with Qt 6 Widgets and Network stack.
  - Zero Python runtime or interpreter overhead.
  - Rapid UI responsiveness and instant cold start.

- **🎮 Full Minecraft Lifecycle Support**:
  - **Mojang Manifest V2 Integration**: Asynchronous query and caching of official releases, snapshots, and legacy beta/alpha versions.
  - **Automated Dependency & Asset Downloader**: Multi-threaded parallel asset, library, and `client.jar` downloads with hash integrity verification and OS/architecture native rule evaluation.
  - **Smart Java Detection**: Automatic scanner locating JRE/JDK installations (Java 8, Java 17, Java 21+) on both Linux (`/usr/lib/jvm`, `PATH`) and Windows (Registry, Program Files), paired with automatic compatibility matching based on Minecraft version requirements.
  - **Modular Launch Engine**: Classpath assembly, native library extraction, customizable JVM arguments (`-Xmx`, `-Xms`, custom GC/flags), and real-time game process supervision.

- **🎨 Modern Dark Interface**:
  - Sleek custom QSS stylesheet with responsive layout.
  - Hero background banner caching.
  - Nickname history and quick selector.
  - Filterable version selector (Releases, Snapshots, Old Beta/Alpha).
  - Quick action buttons (Open `.minecraft` folder, Open Console, Settings).

- **📊 Integrated Console & Real-time Logs**:
  - Live stdout/stderr log viewer capturing game output and launcher debug events.
  - Auto-scrolling, clear logs, and timestamped output.

- **⏱️ Playtime Tracking**:
  - Accurate tracking of total game time persisted in JSON configuration.

- **⚙️ Deep Configuration**:
  - Intuitive Settings dialog for RAM allocation (slider and spinbox).
  - Custom Java executable path override.
  - Customizable window resolution and startup options.

---

## 📁 Project Structure

```
CLauncher/
├── CLauncher.pro              # qmake6 project specification
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
    ├── core/                  # Core utilities and state management
    │   ├── Constants.h        # File paths, URLs, version defaults
    │   ├── Settings.h/.cpp    # JSON-backed configuration manager
    │   ├── Logger.h/.cpp      # Thread-safe logging subsystem
    │   ├── Utils.h/.cpp       # System information, RAM, and path helpers
    │   └── PlaytimeTracker.h/.cpp # Playtime tracking subsystem
    ├── mc/                    # Minecraft game engine
    │   ├── VersionManifest.h/.cpp # Mojang version metadata parser & fetcher
    │   ├── Downloader.h/.cpp  # Parallel asset and library download engine
    │   ├── JavaManager.h/.cpp # JRE scanner and version compatibility
    │   └── GameLauncher.h/.cpp# Classpath builder and process supervisor
    └── ui/                    # Graphical user interface
        ├── MainWindow.h/.cpp  # Main launcher dashboard
        ├── SettingsDialog.h/.cpp # Configuration dialog
        └── ConsoleWindow.h/.cpp  # Real-time game log viewer
```

---

## 🛠️ Quick Start

### Linux Build (Quick)

Ensure `qt6-base-dev` and build tools are installed:

```bash
# Ubuntu / Debian
sudo apt install -y build-essential qt6-base-dev qt6-base-dev-tools

# Arch Linux
sudo pacman -Syu base-devel qt6-base

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
