# ==============================================================================
# CLauncher - Automated Windows PowerShell Installer
# One-liner usage:
#   irm https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.ps1 | iex
# Or from local terminal:
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-Build] [-InstallDir <path>]
# ==============================================================================

[CmdletBinding()]
param(
    [switch]$Build,
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA "Programs\CLauncher"),
    [switch]$NoShortcut,
    [switch]$NoLaunch,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'

function Show-Help {
    Write-Host @"
CLauncher Windows Installer

Usage:
  install.ps1 [-Build] [-InstallDir <path>] [-NoShortcut] [-NoLaunch] [-Help]

Options:
  -Build          Clone the repository and compile CLauncher from source via CMake / MSVC.
  -InstallDir     Custom installation directory (default: %LOCALAPPDATA%\Programs\CLauncher).
  -NoShortcut     Do not create Desktop and Start Menu shortcuts.
  -NoLaunch       Do not prompt to launch CLauncher after installation.
  -Help           Show this help message.

One-liner command:
  irm https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.ps1 | iex
"@
    exit 0
}

if ($Help) {
    Show-Help
}

# --- ANSI Colors & Banner ---
$ESC = [char]27
$C_RESET   = "$ESC[0m"
$C_BOLD    = "$ESC[1m"
$C_CYAN    = "$ESC[96m"
$C_GREEN   = "$ESC[92m"
$C_YELLOW  = "$ESC[93m"
$C_RED     = "$ESC[91m"
$C_GRAY    = "$ESC[90m"
$C_MAGENTA = "$ESC[95m"

Clear-Host
Write-Host "$C_CYAN$C_BOLD"
Write-Host "   ____ _                            _               "
Write-Host "  / ___| |    __ _ _   _ _ __   ___ | |__   ___ _ __ "
Write-Host " | |   | |   / _` | | | | '_ \ / __|| '_ \ / _ \ '__|"
Write-Host " | |___| |__| (_| | |_| | | | | (__ | | | |  __/ |   "
Write-Host "  \____|_____\__,_|\__,_|_| |_|\___||_| |_|\___|_|   "
Write-Host "$C_RESET"
Write-Host "$C_MAGENTA$C_BOLD  ==> CLauncher Windows Automated Installer$C_RESET"
Write-Host "$C_GRAY  High-Performance Native C++20 / Qt6 Minecraft Launcher$C_RESET"
Write-Host "$C_GRAY------------------------------------------------------------------------$C_RESET"
Write-Host ""

# Ensure TLS 1.2 / 1.3 protocol support
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13

# Check 64-bit Architecture
if (-not [Environment]::Is64BitOperatingSystem) {
    Write-Host "$C_RED[ERROR] CLauncher requires a 64-bit Windows operating system.$C_RESET"
    exit 1
}

# Check if CLauncher is currently running
$runningProc = Get-Process -Name "CLauncher" -ErrorAction SilentlyContinue
if ($runningProc) {
    Write-Host "$C_YELLOW[!] Detected running CLauncher process. Terminating to allow update...$C_RESET"
    $runningProc | Stop-Process -Force
    Start-Sleep -Milliseconds 800
}

# Destination target directory
if (-not (Test-Path $InstallDir)) {
    Write-Host "$C_CYAN[*] Creating installation directory: $InstallDir$C_RESET"
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
}

$targetExe = Join-Path $InstallDir "CLauncher.exe"
$icoPath   = Join-Path $InstallDir "CLauncher.ico"

# --- Installation Mode: Build from Source (-Build) or Prebuilt Release ---
if ($Build) {
    Write-Host "$C_CYAN[*] Build Mode Selected: Compiling CLauncher from source...$C_RESET"
    
    # Check for git
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Write-Host "$C_RED[ERROR] Git is required to clone and build CLauncher.$C_RESET"
        Write-Host "Please install Git for Windows: https://git-scm.com/"
        exit 1
    }

    # Check for cmake
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "$C_RED[ERROR] CMake is required to build CLauncher.$C_RESET"
        Write-Host "Please install CMake: https://cmake.org/download/"
        exit 1
    }

    $sourceDir = Join-Path $env:TEMP "CLauncher-Build-$(Get-Random)"
    Write-Host "$C_GRAY  -> Cloning repository into $sourceDir...$C_RESET"
    git clone --depth 1 https://github.com/RanoGames/CLauncher.git $sourceDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host "$C_RED[ERROR] Failed to clone CLauncher repository.$C_RESET"
        exit 1
    }

    Push-Location $sourceDir
    try {
        Write-Host "$C_GRAY  -> Configuring with CMake...$C_RESET"
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) {
            Write-Host "$C_RED[ERROR] CMake configuration failed. Ensure Qt6 and Visual Studio C++ tools are installed.$C_RESET"
            exit 1
        }

        Write-Host "$C_GRAY  -> Building Release target...$C_RESET"
        cmake --build build --config Release --parallel
        if ($LASTEXITCODE -ne 0) {
            Write-Host "$C_RED[ERROR] Compilation failed.$C_RESET"
            exit 1
        }

        $builtExe = $null
        if (Test-Path "build\Release\CLauncher.exe") {
            $builtExe = "build\Release\CLauncher.exe"
        } elseif (Test-Path "build\CLauncher.exe") {
            $builtExe = "build\CLauncher.exe"
        }

        if (-not $builtExe) {
            Write-Host "$C_RED[ERROR] Could not find compiled CLauncher.exe.$C_RESET"
            exit 1
        }

        Write-Host "$C_GRAY  -> Copying built executable to $InstallDir...$C_RESET"
        Copy-Item $builtExe $targetExe -Force
        Copy-Item "resources\mclovin.ico" $icoPath -Force -ErrorAction SilentlyContinue

        if (Get-Command windeployqt.exe -ErrorAction SilentlyContinue) {
            Write-Host "$C_GRAY  -> Running windeployqt for runtime dependencies...$C_RESET"
            windeployqt.exe --release --no-translations --compiler-runtime $targetExe | Out-Null
        }
    } finally {
        Pop-Location
        Remove-Item -Recurse -Force $sourceDir -ErrorAction SilentlyContinue
    }

} else {
    # Default: Download prebuilt release bundle
    Write-Host "$C_CYAN[*] Fetching latest release info from GitHub...$C_RESET"
    $apiUrl = "https://api.github.com/repos/RanoGames/CLauncher/releases/latest"
    $fallbackZipUrl = "https://github.com/RanoGames/CLauncher/releases/latest/download/CLauncher-Windows-x86_64.zip"
    $iconUrl = "https://raw.githubusercontent.com/RanoGames/CLauncher/master/resources/mclovin.ico"

    $downloadUrl = $null
    $releaseTag = "latest"

    try {
        $headers = @{ "User-Agent" = "CLauncher-Installer-PowerShell" }
        $release = Invoke-RestMethod -Uri $apiUrl -Headers $headers -TimeoutSec 8 -ErrorAction Stop
        if ($release.tag_name) {
            $releaseTag = $release.tag_name
        }
        $asset = $release.assets | Where-Object { $_.name -eq "CLauncher-Windows-x86_64.zip" }
        if ($asset -and $asset.browser_download_url) {
            $downloadUrl = $asset.browser_download_url
        }
    } catch {
        # Rate limit or network fallback
        $downloadUrl = $fallbackZipUrl
    }

    if (-not $downloadUrl) {
        $downloadUrl = $fallbackZipUrl
    }

    $tempZip = Join-Path $env:TEMP "CLauncher-Windows-x86_64-$releaseTag.zip"
    Write-Host "$C_GRAY  -> Downloading CLauncher $releaseTag package...$C_RESET"
    
    $prevProgress = $ProgressPreference
    $ProgressPreference = 'SilentlyContinue'
    try {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $tempZip -UseBasicParsing
    } finally {
        $ProgressPreference = $prevProgress
    }

    if (-not (Test-Path $tempZip) -or ((Get-Item $tempZip).Length -lt 1000000)) {
        Write-Host "$C_RED[ERROR] Downloaded package appears incomplete or corrupt.$C_RESET"
        exit 1
    }

    Write-Host "$C_GRAY  -> Extracting files to $InstallDir...$C_RESET"
    Expand-Archive -Path $tempZip -DestinationPath $InstallDir -Force
    Remove-Item -Path $tempZip -Force -ErrorAction SilentlyContinue

    # Ensure icon is downloaded for crisp shortcut rendering
    if (-not (Test-Path $icoPath)) {
        try {
            Invoke-WebRequest -Uri $iconUrl -OutFile $icoPath -UseBasicParsing -ErrorAction SilentlyContinue
        } catch { }
    }
}

# Validate CLauncher.exe location
if (-not (Test-Path $targetExe)) {
    $found = Get-ChildItem -Path $InstallDir -Filter "CLauncher.exe" -Recurse | Select-Object -First 1
    if ($found) {
        $targetExe = $found.FullName
        $InstallDir = $found.DirectoryName
    } else {
        Write-Host "$C_RED[ERROR] CLauncher.exe not found in $InstallDir after installation.$C_RESET"
        exit 1
    }
}

# --- Shortcuts Creation (Desktop & Start Menu) ---
if (-not $NoShortcut) {
    Write-Host "$C_CYAN[*] Creating system shortcuts...$C_RESET"
    try {
        $wsh = New-Object -ComObject WScript.Shell
        
        $shortcutIcon = if (Test-Path $icoPath) { $icoPath } else { "$targetExe,0" }

        # Desktop Shortcut
        $desktopDir = [Environment]::GetFolderPath("Desktop")
        if (Test-Path $desktopDir) {
            $deskShortcut = $wsh.CreateShortcut((Join-Path $desktopDir "CLauncher.lnk"))
            $deskShortcut.TargetPath = $targetExe
            $deskShortcut.WorkingDirectory = $InstallDir
            $deskShortcut.IconLocation = $shortcutIcon
            $deskShortcut.Description = "CLauncher - High Performance Minecraft Launcher"
            $deskShortcut.Save()
            Write-Host "$C_GRAY  -> Desktop shortcut created.$C_RESET"
        }

        # Start Menu Shortcut
        $programsDir = [Environment]::GetFolderPath("Programs")
        $startMenuFolder = Join-Path $programsDir "CLauncher"
        if (-not (Test-Path $startMenuFolder)) {
            New-Item -ItemType Directory -Force -Path $startMenuFolder | Out-Null
        }
        $startShortcut = $wsh.CreateShortcut((Join-Path $startMenuFolder "CLauncher.lnk"))
        $startShortcut.TargetPath = $targetExe
        $startShortcut.WorkingDirectory = $InstallDir
        $startShortcut.IconLocation = $shortcutIcon
        $startShortcut.Description = "CLauncher - High Performance Minecraft Launcher"
        $startShortcut.Save()
        Write-Host "$C_GRAY  -> Start Menu shortcut created.$C_RESET"

    } catch {
        Write-Host "$C_YELLOW[!] Note: Shortcut creation skipped ($($_.Exception.Message)).$C_RESET"
    }
}

# --- Add to User PATH ---
try {
    $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    $pathList = ($userPath -split ';') | Where-Object { $_ -and (Test-Path $_) }
    if ($pathList -notcontains $InstallDir) {
        Write-Host "$C_CYAN[*] Adding CLauncher to User PATH environment variable...$C_RESET"
        $newPath = ($pathList + $InstallDir) -join ';'
        [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
        $env:PATH = "$env:PATH;$InstallDir"
        Write-Host "$C_GRAY  -> Added to PATH.$C_RESET"
    }
} catch {
    Write-Host "$C_YELLOW[!] Could not automatically update PATH: $($_.Exception.Message)$C_RESET"
}

# --- Summary & Launch Prompt ---
Write-Host ""
Write-Host "$C_GREEN$C_BOLD[+] CLauncher installed successfully!$C_RESET"
Write-Host "    $C_BOLDLocation:$C_RESET    $InstallDir"
Write-Host "    $C_BOLDExecutable:$C_RESET  $targetExe"
Write-Host ""

if (-not $NoLaunch) {
    $canPrompt = [Environment]::UserInteractive -and -not [Console]::IsInputRedirected
    if ($canPrompt) {
        try {
            $choice = Read-Host "Launch CLauncher now? [Y/n]"
            if ($choice -eq "" -or $choice -match "^[yY]") {
                Write-Host "$C_GREEN[*] Starting CLauncher... Enjoy playing!$C_RESET"
                Start-Process -FilePath $targetExe -WorkingDirectory $InstallDir
            }
        } catch { }
    } else {
        Write-Host "$C_GRAYRun 'CLauncher' or use the desktop shortcut to play.$C_RESET"
    }
}
