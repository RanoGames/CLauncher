# Building CLauncher

CLauncher is a high-performance, native C++20 / Qt6 Minecraft launcher. This document details prerequisites, environment setup, and build procedures for Linux and Windows (native without WSL), as well as cross-compilation options.

---

## Table of Contents
1. [Linux Build Instructions](#linux-build-instructions)
   - [Prerequisites & Package Managers](#prerequisites--package-managers)
   - [Building with qmake6](#building-with-qmake6)
   - [Building with CMake](#building-with-cmake)
2. [Windows Native Build Instructions (No WSL)](#windows-native-build-instructions-no-wsl)
   - [Prerequisites](#prerequisites)
   - [Method 1: Qt Creator (Recommended GUI)](#method-1-qt-creator-recommended-gui)
   - [Method 2: Visual Studio 2022 / MSVC CLI](#method-2-visual-studio-2022--msvc-cli)
   - [Method 3: MinGW 64-bit CLI](#method-3-mingw-64-bit-cli)
   - [Deploying on Windows with windeployqt](#deploying-on-windows-with-windeployqt)
3. [Cross-Compilation (Linux to Windows)](#cross-compilation-linux-to-windows)
   - [Using MinGW-w64 & MXE](#using-mingw-w64--mxe)
   - [Using Docker / Pre-packaged Toolchains](#using-docker--pre-packaged-toolchains)
4. [Troubleshooting & FAQ](#troubleshooting--faq)

---

## Linux Build Instructions

### Prerequisites & Package Managers

Ensure your system has a modern C++ compiler (`g++` >= 11 or `clang++` >= 13), `make` or `ninja`, Qt 6 development libraries (`core`, `gui`, `widgets`, `network`), and `zlib` development headers (used for in-memory ZIP central directory and mod metadata decompression).

#### Ubuntu / Debian (22.04 LTS, 24.04 LTS+)
```bash
sudo apt update
sudo apt install -y build-essential \
                    qt6-base-dev \
                    qt6-base-dev-tools \
                    libqt6network6 \
                    libqt6widgets6 \
                    libqt6gui6 \
                    libqt6core6t64 \
                    zlib1g-dev \
                    cmake ninja-build
```

#### Fedora / RHEL
```bash
sudo dnf install -y gcc-c++ make cmake ninja-build \
                    qt6-qtbase-devel \
                    qt6-qtbase-gui \
                    zlib-devel
```

#### Arch Linux / Manjaro
```bash
sudo pacman -Syu --needed base-devel qt6-base zlib cmake ninja
```

---

### Building with qmake6

1. Navigate to the project root:
   ```bash
   cd /path/to/CLauncher
   ```

2. Generate the Makefile:
   ```bash
   qmake6 CLauncher.pro
   ```
   *(On some distributions where Qt6 is the default, `qmake` points to `qmake6`)*.

3. Compile with parallel jobs:
   ```bash
   make -j$(nproc)
   ```

4. Run CLauncher:
   ```bash
   ./CLauncher
   ```

To clean the build:
```bash
make clean
rm -f Makefile .qmake.stash CLauncher
```

---

### Building with CMake

1. Create and enter a build directory:
   ```bash
   cd /path/to/CLauncher
   mkdir -p build && cd build
   ```

2. Configure the project:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release ..
   ```

3. Build:
   ```bash
   cmake --build . --parallel $(nproc)
   ```

4. Run the executable:
   ```bash
   ./CLauncher
   ```

---

## Windows Native Build Instructions (No WSL)

CLauncher runs natively on Windows 10/11 (64-bit) without requiring WSL, Cygwin, or virtual machines.

### Prerequisites

1. **Qt 6 Framework**:
   - Download the **Qt Online Installer** from [qt.io/download](https://www.qt.io/download).
   - In the installer component selector, expand `Qt` -> `Qt 6.x.x` (e.g. Qt 6.6, 6.7, or 6.8 LTS).
   - Check one or both toolchains:
     - `MSVC 2022 64-bit` (Recommended if you have Visual Studio)
     - `MinGW 11.2.0 (or newer) 64-bit`
   - Under `Developer and Designer Tools`, select `Qt Creator` and `CMake`.

2. **C++ Compiler & Libraries**:
   - **Visual Studio 2022 Community** (or Professional/Enterprise): Check "Desktop development with C++".
   - OR **MinGW-w64** (included in Qt installer or via MSYS2: `pacman -S mingw-w64-x86_64-zlib`).
   - For MSVC CLI / CMake builds, install `zlib` via vcpkg (`vcpkg install zlib:x64-windows`) or link Qt's internal bundled zlib.

---

### Method 1: Qt Creator (Recommended GUI)

1. Launch **Qt Creator**.
2. Go to **File** -> **Open File or Project...** and select `CLauncher.pro` (or `CMakeLists.txt`).
3. Select your installed kit (e.g., `Desktop Qt 6.x.x MSVC2022 64bit` or `Desktop Qt 6.x.x MinGW 64-bit`).
4. Click **Configure Project**.
5. Switch build configuration to **Release** (bottom-left computer icon).
6. Press `Ctrl + B` to build, or `Ctrl + R` to build and run.

---

### Method 2: Visual Studio 2022 / MSVC CLI

1. Open the **x64 Native Tools Command Prompt for VS 2022** from your Start Menu.
2. Add Qt binaries and tools to your `PATH`:
   ```cmd
   set PATH=C:\Qt\6.7.2\msvc2022_64\bin;%PATH%
   ```
3. Navigate to CLauncher:
   ```cmd
   cd C:\path\to\CLauncher
   ```
4. **Using qmake**:
   ```cmd
   qmake CLauncher.pro
   nmake
   ```
5. **Or using CMake**:
   ```cmd
   mkdir build
   cd build
   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.7.2\msvc2022_64" ..
   nmake
   ```

The compiled `CLauncher.exe` will be located in the release or current directory.

---

### Method 3: MinGW 64-bit CLI

1. Open standard Command Prompt (`cmd.exe`) or PowerShell.
2. Add Qt and MinGW to your `PATH`:
   ```cmd
   set PATH=C:\Qt\6.7.2\mingw_64\bin;C:\Qt\Tools\mingw1120_64\bin;%PATH%
   ```
3. Navigate to CLauncher:
   ```cmd
   cd C:\path\to\CLauncher
   ```
4. Run `qmake` and `mingw32-make`:
   ```cmd
   qmake CLauncher.pro
   mingw32-make -j%NUMBER_OF_PROCESSORS%
   ```

---

### Deploying on Windows with windeployqt

Qt applications on Windows require dynamic link libraries (DLLs) and Qt plugins (`platforms/qwindows.dll`, `styles`, `imageformats`) to run outside of the development environment.

Qt provides `windeployqt` to bundle everything automatically:

1. Create a distribution folder and copy `CLauncher.exe` into it:
   ```cmd
   mkdir C:\CLauncher-Dist
   copy release\CLauncher.exe C:\CLauncher-Dist\
   cd C:\CLauncher-Dist
   ```

2. Run `windeployqt`:
   ```cmd
   windeployqt --release --compiler-runtime CLauncher.exe
   ```

3. Resulting standalone folder:
   ```
   CLauncher-Dist/
   ├── CLauncher.exe
   ├── Qt6Core.dll
   ├── Qt6Gui.dll
   ├── Qt6Widgets.dll
   ├── Qt6Network.dll
   ├── platforms/
   │   └── qwindows.dll
   ├── styles/
   ├── imageformats/
   └── vc_redist.x64.exe (or MSVC/MinGW runtime DLLs)
   ```
This folder can now be zipped and distributed to any Windows PC without requiring Qt or development tools installed.

---

## Cross-Compilation (Linux to Windows)

You can compile Windows `.exe` binaries directly from Linux using the MinGW-w64 toolchain and Qt6 Windows libraries.

### Using Arch Linux / AUR (Mingw-w64)
Arch Linux provides pre-compiled Qt6 Windows packages in AUR:
```bash
yay -S mingw-w64-gcc mingw-w64-qt6-base
x86_64-w64-mingw32-qmake-qt6 CLauncher.pro
make -j$(nproc)
```

### Using CMake Toolchain File
Create a `toolchain-mingw64.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /opt/mxe/usr/x86_64-w64-mingw32.shared)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```
Build using:
```bash
mkdir build-win && cd build-win
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

---

## Ported Modules & Architecture

CLauncher incorporates complete native C++ implementations of all essential Minecraft launcher features:

| Module | Files | Purpose / Key Features |
|---|---|---|
| **Discord RPC** | `src/mc/DiscordRpc.h/.cpp` | Native QLocalSocket IPC client for Discord Rich Presence ($XDG_RUNTIME_DIR on Linux, named pipes on Windows). Handshake opcode 0, SET_ACTIVITY opcode 1. |
| **Mod Manager** | `src/mc/ModManager.h/.cpp`<br>`src/ui/ModManagerDialog.h/.cpp` | In-memory ZIP central directory inspector (zlib DEFLATE), `.jar` <-> `.jar.disabled` toggle, metadata extraction (fabric, quilt, forge, neoforge), UI manager dialog. |
| **Modrinth Browser** | `src/mc/ModBrowser.h/.cpp`<br>`src/ui/ModBrowserDialog.h/.cpp` | Modrinth v2 REST API integration, query & facet filtering, version matching, 1-click download with progress reporting. |
| **Skin Avatar System** | `src/mc/SkinManager.h/.cpp`<br>`src/ui/SkinPreviewWidget.h/.cpp` | Ely.by and Mojang texture fetching, local cache, 8x8 face extraction with outer helmet composite, antialiased 2D avatar preview widget. |
| **Crash Analyzer** | `src/core/CrashAnalyzer.h/.cpp` | Automated crash diagnostics parsing exit codes, `crash-reports/*.txt`, and `latest.log`. Diagnoses OOM, GPU/OpenGL, missing Java, and mod conflicts. |
| **Adoptium JRE Downloader** | `src/mc/JavaDownloader.h/.cpp` | Automated portable JRE discovery, download, and extraction from Adoptium Temurin v3 API into `runtime/` for required Java versions (8, 17, 21). |
| **Fabric Installer** | `src/mc/ModloaderInstaller.h/.cpp` | Fabric Meta API queries, version manifest generation, and automated installation into `versions/`. |
| **Updater** | `src/core/Updater.h/.cpp` | GitHub Releases API update checker with semantic version comparison and direct asset downloading. |

---

## Troubleshooting & FAQ

### 1. `qmake6: command not found`
On Debian/Ubuntu, `qmake6` is part of `qt6-base-dev-tools`. If only `qmake` is present, check `qmake -v` to ensure it targets Qt 6. You can also specify the full path: `/usr/lib/qt6/bin/qmake`.

### 2. Missing OpenGL / XCB libraries on Linux
If you encounter errors like `Could not find the Qt platform plugin "xcb"`:
```bash
sudo apt install libxcb-cursor0 libxcb-xinerama0 libxcb-shape0
```

### 3. Missing `platforms/qwindows.dll` on Windows
Make sure you ran `windeployqt` as described in the deployment section. The `platforms` subfolder must be in the same directory as `CLauncher.exe`.

### 4. Headless Smoke Testing
To test the binary without an X server or GUI display:
```bash
QT_QPA_PLATFORM=offscreen ./CLauncher --help
```
