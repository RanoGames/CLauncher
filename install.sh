#!/usr/bin/env bash
# ==============================================================================
# CLauncher - Automated Linux Installer
# One-liner usage:
#   curl -fsSL https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.sh | bash
# Or locally:
#   chmod +x install.sh
#   ./install.sh [--build] [--prefix <path>] [--no-desktop] [--no-launch] [--dry-run]
# ==============================================================================

set -eo pipefail

# --- CLI Color Configuration ---
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RESET="\033[0m"
    C_BOLD="\033[1m"
    C_CYAN="\033[38;5;51m"
    C_GREEN="\033[38;5;82m"
    C_YELLOW="\033[38;5;220m"
    C_RED="\033[38;5;196m"
    C_MAGENTA="\033[38;5;201m"
    C_GRAY="\033[38;5;244m"
else
    C_RESET=""
    C_BOLD=""
    C_CYAN=""
    C_GREEN=""
    C_YELLOW=""
    C_RED=""
    C_MAGENTA=""
    C_GRAY=""
fi

# --- Default Options ---
DO_BUILD=false
PREFIX="${HOME}/.local"
NO_DESKTOP=false
NO_LAUNCH=false
DRY_RUN=false

show_help() {
    cat <<EOF
CLauncher Linux Installer

Usage:
  install.sh [options]
  curl -fsSL https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.sh | bash -s -- [options]

Options:
  -b, --build       Clone and compile CLauncher from source via CMake/Ninja or qmake6.
  -p, --prefix DIR  Install directory prefix (default: \$HOME/.local -> ~/.local/bin/clauncher).
  --no-desktop      Do not create desktop launcher and icon.
  --no-launch       Do not prompt to launch CLauncher after installation.
  --dry-run         Simulate installation steps without writing files.
  -h, --help        Show this help message.
EOF
    exit 0
}

# Parse command line options
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build)
            DO_BUILD=true
            shift
            ;;
        -p|--prefix)
            PREFIX="$2"
            shift 2
            ;;
        --no-desktop)
            NO_DESKTOP=true
            shift
            ;;
        --no-launch)
            NO_LAUNCH=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo -e "${C_RED}[ERROR] Unknown option: $1${C_RESET}"
            show_help
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || echo "")"
BIN_DIR="${PREFIX}/bin"
APPS_DIR="${PREFIX}/share/applications"
ICONS_DIR="${PREFIX}/share/icons/hicolor/256x256/apps"
PIXMAPS_DIR="${PREFIX}/share/pixmaps"
TARGET_BIN="${BIN_DIR}/clauncher"
DESKTOP_FILE="${APPS_DIR}/clauncher.desktop"
ICON_FILE="${ICONS_DIR}/clauncher.png"
PIXMAP_FILE="${PIXMAPS_DIR}/clauncher.png"

# --- Banner ---
echo -e "${C_CYAN}${C_BOLD}"
cat << "EOF"
   ____ _                            _               
  / ___| |    __ _ _   _ _ __   ___ | |__   ___ _ __ 
 | |   | |   / _` | | | | '_ \ / __|| '_ \ / _ \ '__|
 | |___| |__| (_| | |_| | | | | (__ | | | |  __/ |   
  \____|_____\__,_|\__,_|_| |_|\___||_| |_|\___|_|   
EOF
echo -e "${C_RESET}"
echo -e "${C_MAGENTA}${C_BOLD}  ==> CLauncher Linux Automated Installer${C_RESET}"
echo -e "${C_GRAY}  High-Performance Native C++20 / Qt6 Minecraft Launcher${C_RESET}"
echo -e "${C_GRAY}------------------------------------------------------------------------${C_RESET}"
echo ""

# --- Architecture Check ---
ARCH="$(uname -m)"
if [[ "$ARCH" != "x86_64" && "$ARCH" != "amd64" ]]; then
    if [[ "$DO_BUILD" != "true" ]]; then
        echo -e "${C_RED}[ERROR] Prebuilt Linux binaries are only provided for x86_64 (detected: ${ARCH}).${C_RESET}"
        echo -e "You can compile CLauncher natively from source on your system by running:"
        echo -e "  ${C_CYAN}curl -fsSL https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.sh | bash -s -- --build${C_RESET}"
        exit 1
    fi
fi

# Setup cleanup on exit
TMP_DIR=""
cleanup() {
    if [[ -n "$TMP_DIR" && -d "$TMP_DIR" ]]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT INT TERM

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${C_YELLOW}[DRY-RUN MODE] Simulating installation without filesystem modifications.${C_RESET}"
fi

# --- Mode 1: Build from Source ---
if [[ "$DO_BUILD" == "true" ]]; then
    echo -e "${C_CYAN}[*] Build mode active: Compiling CLauncher from source...${C_RESET}"

    # Verify git
    if ! command -v git >/dev/null 2>&1; then
        echo -e "${C_RED}[ERROR] git is required to clone and build CLauncher.${C_RESET}"
        exit 1
    fi

    # Check for compiler and build system
    HAS_CMAKE=false
    HAS_QMAKE=false
    command -v cmake >/dev/null 2>&1 && HAS_CMAKE=true
    command -v qmake6 >/dev/null 2>&1 && HAS_QMAKE=true

    if ! $HAS_CMAKE && ! $HAS_QMAKE; then
        echo -e "${C_RED}[ERROR] Neither 'cmake' nor 'qmake6' was found on your system.${C_RESET}"
        echo -e "Please install required build packages:"
        if [ -f /etc/debian_version ]; then
            echo -e "  ${C_CYAN}sudo apt update && sudo apt install -y build-essential qt6-base-dev qt6-base-dev-tools zlib1g-dev cmake ninja-build${C_RESET}"
        elif [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then
            echo -e "  ${C_CYAN}sudo dnf install -y gcc-c++ make cmake ninja-build qt6-qtbase-devel zlib-devel${C_RESET}"
        elif [ -f /etc/arch-release ]; then
            echo -e "  ${C_CYAN}sudo pacman -Syu --needed base-devel qt6-base zlib cmake ninja${C_RESET}"
        fi
        exit 1
    fi

    if [[ "$DRY_RUN" == "false" ]]; then
        TMP_DIR="$(mktemp -d -t clauncher-build-XXXXXX)"
        echo -e "${C_GRAY}  -> Cloning repository into temporary directory...${C_RESET}"
        git clone --depth 1 https://github.com/RanoGames/CLauncher.git "${TMP_DIR}/source"
        cd "${TMP_DIR}/source"

        if $HAS_CMAKE; then
            echo -e "${C_GRAY}  -> Configuring with CMake...${C_RESET}"
            cmake -B build -DCMAKE_BUILD_TYPE=Release
            echo -e "${C_GRAY}  -> Building Release target...${C_RESET}"
            cmake --build build --config Release -j"$(nproc 2>/dev/null || echo 2)"
            BUILT_BIN="${TMP_DIR}/source/build/CLauncher"
        else
            echo -e "${C_GRAY}  -> Configuring with qmake6...${C_RESET}"
            qmake6 CLauncher.pro
            echo -e "${C_GRAY}  -> Compiling CLauncher...${C_RESET}"
            make -j"$(nproc 2>/dev/null || echo 2)"
            BUILT_BIN="${TMP_DIR}/source/CLauncher"
        fi

        if [[ ! -f "$BUILT_BIN" ]]; then
            echo -e "${C_RED}[ERROR] Build finished but binary was not found.${C_RESET}"
            exit 1
        fi
    fi

else
    # --- Mode 2: Prebuilt Release Download ---
    echo -e "${C_CYAN}[*] Resolving latest CLauncher release from GitHub...${C_RESET}"

    API_URL="https://api.github.com/repos/RanoGames/CLauncher/releases/latest"
    FALLBACK_URL="https://github.com/RanoGames/CLauncher/releases/latest/download/CLauncher-Linux-x86_64"
    DOWNLOAD_URL=""
    RELEASE_TAG="latest"

    if command -v curl >/dev/null 2>&1; then
        HTTP_RESP="$(curl -sSL -H "User-Agent: CLauncher-Installer-Linux" "$API_URL" 2>/dev/null || true)"
        if [[ -n "$HTTP_RESP" ]] && ! echo "$HTTP_RESP" | grep -qi "API rate limit exceeded"; then
            RELEASE_TAG="$(echo "$HTTP_RESP" | grep -o '"tag_name": *"[^"]*"' | head -n1 | cut -d'"' -f4 || echo "latest")"
            DOWNLOAD_URL="$(echo "$HTTP_RESP" | grep -o 'https://[^"]*CLauncher-Linux-x86_64' | head -n1 || true)"
        fi
    fi

    if [[ -z "$DOWNLOAD_URL" ]]; then
        DOWNLOAD_URL="$FALLBACK_URL"
    fi

    echo -e "${C_GRAY}  -> Version: ${C_BOLD}${RELEASE_TAG}${C_RESET}"
    echo -e "${C_GRAY}  -> URL:     ${DOWNLOAD_URL}${C_RESET}"

    if [[ "$DRY_RUN" == "false" ]]; then
        TMP_DIR="$(mktemp -d -t clauncher-install-XXXXXX)"
        DOWNLOADED_BIN="${TMP_DIR}/CLauncher-Linux-x86_64"

        echo -e "${C_CYAN}[*] Downloading CLauncher executable...${C_RESET}"
        curl -fSL --progress-bar -o "$DOWNLOADED_BIN" "$DOWNLOAD_URL"

        if [[ ! -s "$DOWNLOADED_BIN" ]]; then
            echo -e "${C_RED}[ERROR] Download failed or received empty payload.${C_RESET}"
            exit 1
        fi

        BUILT_BIN="$DOWNLOADED_BIN"
    fi
fi

# --- Install Executable ---
if [[ "$DRY_RUN" == "false" ]]; then
    echo -e "${C_CYAN}[*] Installing binary into ${BIN_DIR}...${C_RESET}"
    mkdir -p "$BIN_DIR"
    
    # Close any running instance
    if pgrep -x "clauncher" >/dev/null 2>&1 || pgrep -x "CLauncher" >/dev/null 2>&1; then
        echo -e "${C_YELLOW}[!] Detected running CLauncher process. Stopping...${C_RESET}"
        pkill -x "clauncher" 2>/dev/null || true
        pkill -x "CLauncher" 2>/dev/null || true
        sleep 0.5
    fi

    cp -f "$BUILT_BIN" "$TARGET_BIN"
    chmod 755 "$TARGET_BIN"
else
    echo -e "${C_GRAY}  [dry-run] Would copy binary to ${TARGET_BIN}${C_RESET}"
fi

# --- Desktop Integration & Icon ---
if [[ "$NO_DESKTOP" == "false" ]]; then
    echo -e "${C_CYAN}[*] Installing desktop shortcut and icons...${C_RESET}"
    
    if [[ "$DRY_RUN" == "false" ]]; then
        mkdir -p "$APPS_DIR" "$ICONS_DIR" "$PIXMAPS_DIR"

        # Download or copy icon
        RAW_BASE="https://raw.githubusercontent.com/RanoGames/CLauncher/master/resources"
        FOUND_ICON=false

        if [[ -f "${TMP_DIR}/source/resources/clauncher.png" ]]; then
            cp -f "${TMP_DIR}/source/resources/clauncher.png" "$ICON_FILE"
            FOUND_ICON=true
        elif [[ -n "$SCRIPT_DIR" && -f "$SCRIPT_DIR/resources/clauncher.png" ]]; then
            cp -f "$SCRIPT_DIR/resources/clauncher.png" "$ICON_FILE"
            FOUND_ICON=true
        fi

        if ! $FOUND_ICON; then
            if curl -fsSL -o "$ICON_FILE" "${RAW_BASE}/clauncher.png" 2>/dev/null && [[ -s "$ICON_FILE" ]]; then
                FOUND_ICON=true
            fi
        fi

        if ! $FOUND_ICON; then
            TMP_ICO="${TMP_DIR}/mclovin.ico"
            if [[ -f "${TMP_DIR}/source/resources/mclovin.ico" ]]; then
                cp -f "${TMP_DIR}/source/resources/mclovin.ico" "$TMP_ICO"
            elif [[ -n "$SCRIPT_DIR" && -f "$SCRIPT_DIR/resources/mclovin.ico" ]]; then
                cp -f "$SCRIPT_DIR/resources/mclovin.ico" "$TMP_ICO"
            else
                curl -fsSL -o "$TMP_ICO" "${RAW_BASE}/mclovin.ico" 2>/dev/null || true
            fi

            if [[ -s "$TMP_ICO" ]] && command -v python3 >/dev/null 2>&1; then
                python3 -c "
import re, struct
with open('$TMP_ICO', 'rb') as f:
    data = f.read()
max_w, best_png = 0, None
for m in re.finditer(b'\x89PNG\r\n\x1a\n', data):
    off = m.start()
    w, h = struct.unpack('>II', data[off+16:off+24])
    iend = data.find(b'IEND', off) + 8
    if w >= max_w and iend > off:
        max_w = w
        best_png = data[off:iend]
if best_png:
    with open('$ICON_FILE', 'wb') as out:
        out.write(best_png)
" 2>/dev/null || true
                if [[ -s "$ICON_FILE" ]]; then
                    FOUND_ICON=true
                fi
            fi
        fi

        if $FOUND_ICON; then
            cp -f "$ICON_FILE" "$PIXMAP_FILE" 2>/dev/null || true
        fi

        # Generate .desktop file
        cat > "$DESKTOP_FILE" << EOF
[Desktop Entry]
Type=Application
Name=CLauncher
GenericName=Minecraft Launcher
Comment=High-performance native C++/Qt6 Minecraft Launcher
Exec=${TARGET_BIN} %u
Icon=clauncher
Terminal=false
Categories=Game;ActionGame;AdventureGame;
Keywords=minecraft;game;launcher;mojang;tlauncher;clauncher;
StartupNotify=true
StartupWMClass=CLauncher
MimeType=x-scheme-handler/minecraft;
EOF
        chmod 644 "$DESKTOP_FILE"

        # Update desktop database & icon cache if available
        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database "$APPS_DIR" 2>/dev/null || true
        fi
        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true
        fi
        echo -e "${C_GRAY}  -> Desktop file: ${DESKTOP_FILE}${C_RESET}"
        echo -e "${C_GRAY}  -> App icon:     ${ICON_FILE}${C_RESET}"
    else
        echo -e "${C_GRAY}  [dry-run] Would create ${DESKTOP_FILE}${C_RESET}"
        echo -e "${C_GRAY}  [dry-run] Would save icon to ${ICON_FILE}${C_RESET}"
    fi
fi

# --- Verify PATH ---
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    echo ""
    echo -e "${C_YELLOW}[!] Notice: ${BIN_DIR} is not in your current PATH.${C_RESET}"
    echo -e "To launch CLauncher simply by typing ${C_BOLD}clauncher${C_RESET} in any terminal, add this to your ${C_CYAN}~/.bashrc${C_RESET} or ${C_CYAN}~/.zshrc${C_RESET}:"
    echo -e "  ${C_CYAN}export PATH=\"${BIN_DIR}:\$PATH\"${C_RESET}"
fi

# --- Summary ---
echo ""
echo -e "${C_GREEN}${C_BOLD}[+] CLauncher installation complete!${C_RESET}"
echo -e "    ${C_BOLD}Binary:${C_RESET}      ${TARGET_BIN}"
if [[ "$NO_DESKTOP" == "false" ]]; then
    echo -e "    ${C_BOLD}Desktop:${C_RESET}     ${DESKTOP_FILE}"
fi
echo ""

# --- Prompt to Launch ---
if [[ "$NO_LAUNCH" == "false" && "$DRY_RUN" == "false" ]]; then
    # When executed via `curl ... | bash`, stdin is redirected from curl.
    # We read user choice from /dev/tty if available.
    if [ -r /dev/tty ]; then
        printf "${C_BOLD}Launch CLauncher now? [Y/n]: ${C_RESET}"
        read -r response </dev/tty || response="n"
        if [[ -z "$response" || "$response" =~ ^[Yy]$ ]]; then
            echo -e "${C_GREEN}[*] Launching CLauncher... Have fun!${C_RESET}"
            nohup "$TARGET_BIN" >/dev/null 2>&1 &
        fi
    fi
fi
