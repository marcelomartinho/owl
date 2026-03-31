# Owl Browser - Fetch Chromium Source (Windows)
# Downloads and prepares the Chromium source tree for Owl modifications.
# Requires: Git, Python 3.9+, ~100GB free disk space
param(
    [string]$ChromiumDir = "",
    [string]$ChromiumVersion = "134.0.6998.0"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OwlRoot = Split-Path -Parent $ScriptDir

if (-not $ChromiumDir) {
    $ChromiumDir = Join-Path $OwlRoot "chromium"
}

function Write-Info  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Green }
function Write-Warn  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Yellow }
function Write-Err   { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Red }

function Test-Dependencies {
    $missing = @()
    foreach ($cmd in @("git", "python")) {
        if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
            $missing += $cmd
        }
    }
    if ($missing.Count -gt 0) {
        Write-Err "Missing dependencies: $($missing -join ', ')"
        Write-Err "Install them and ensure they are in PATH."
        exit 1
    }
}

function Install-DepotTools {
    $depotToolsDir = Join-Path $OwlRoot "depot_tools"

    if (Test-Path $depotToolsDir) {
        Write-Info "depot_tools already installed, updating..."
        Push-Location $depotToolsDir
        git pull --quiet
        Pop-Location
        return
    }

    Write-Info "Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $depotToolsDir
}

function Fetch-ChromiumSource {
    $depotToolsDir = Join-Path $OwlRoot "depot_tools"
    $env:PATH = "$depotToolsDir;$env:PATH"
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

    $chromiumSrc = Join-Path $ChromiumDir "src"
    if (Test-Path $chromiumSrc) {
        Write-Warn "Chromium source already exists at $chromiumSrc"
        Write-Info "To re-fetch, remove $ChromiumDir and run again."
        return
    }

    Write-Info "Fetching Chromium source (this will take a while, ~30GB)..."
    New-Item -ItemType Directory -Force -Path $ChromiumDir | Out-Null
    Push-Location $ChromiumDir

    $gclientContent = @"
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git@$ChromiumVersion",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_nacl": False,
      "checkout_telemetry_dependencies": False,
    },
  },
]
"@
    Set-Content -Path ".gclient" -Value $gclientContent

    Write-Info "Running gclient sync (downloading source + dependencies)..."
    $cpuCount = (Get-CimInstance -ClassName Win32_Processor).NumberOfLogicalProcessors
    & gclient sync --nohooks --no-history -j$cpuCount
    if ($LASTEXITCODE -ne 0) { throw "gclient sync failed" }

    Write-Info "Running hooks..."
    Push-Location "src"
    & gclient runhooks
    if ($LASTEXITCODE -ne 0) { throw "gclient runhooks failed" }
    Pop-Location
    Pop-Location
}

function Test-VisualStudio {
    Write-Info "Checking Visual Studio installation..."

    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        Write-Err "Visual Studio not found!"
        Write-Err "Install Visual Studio 2022 with 'Desktop development with C++' workload."
        Write-Err "Download: https://visualstudio.microsoft.com/downloads/"
        exit 1
    }

    $vsPath = & $vsWhere -latest -property installationPath
    Write-Info "Found Visual Studio at: $vsPath"

    # Check for Windows SDK
    $sdkPath = "${env:ProgramFiles(x86)}\Windows Kits\10\Include"
    if (-not (Test-Path $sdkPath)) {
        Write-Warn "Windows 10/11 SDK not found. It may be needed for building."
        Write-Warn "Install via Visual Studio Installer -> Individual Components -> Windows SDK"
    }
}

function Main {
    Write-Info "=== Owl Browser - Chromium Source Fetch (Windows) ==="
    Write-Info "Target version: $ChromiumVersion"
    Write-Info "Target directory: $ChromiumDir"
    Write-Host ""

    Test-Dependencies
    Test-VisualStudio
    Install-DepotTools
    Fetch-ChromiumSource

    Write-Host ""
    Write-Info "=== Chromium source ready! ==="
    Write-Info "Next steps:"
    Write-Info "  1. .\scripts\apply_patches_win.ps1   # Apply Owl patches"
    Write-Info "  2. .\scripts\build_win.ps1            # Build Owl browser"
}

Main
