# Owl Browser - Build Script (Windows)
# Configures GN args and compiles the Owl browser on Windows.
#
# Prerequisites:
#   - Visual Studio 2022 with "Desktop development with C++" workload
#   - Windows 10/11 SDK (10.0.22621.0+)
#   - depot_tools in PATH
#   - Chromium source fetched and patches applied
param(
    [ValidateSet("debug", "release", "official")]
    [string]$BuildType = "release",

    [string]$BuildDir = "out\Owl",

    [int]$Jobs = 0,

    [string]$ChromiumDir = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OwlRoot = Split-Path -Parent $ScriptDir

if (-not $ChromiumDir) {
    $ChromiumDir = Join-Path $OwlRoot "chromium"
}
$ChromiumSrc = Join-Path $ChromiumDir "src"

function Write-Info  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Green }
function Write-Warn  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Yellow }
function Write-Err   { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Red }

function Test-Environment {
    # Add depot_tools to PATH
    $depotToolsDir = Join-Path $OwlRoot "depot_tools"
    $env:PATH = "$depotToolsDir;$env:PATH"
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

    if (-not (Test-Path $ChromiumSrc)) {
        Write-Err "Chromium source not found at $ChromiumSrc"
        Write-Err "Run .\scripts\fetch_chromium_win.ps1 first."
        exit 1
    }

    if (-not (Get-Command "gn" -ErrorAction SilentlyContinue)) {
        Write-Err "GN not found. Ensure depot_tools is in PATH."
        exit 1
    }

    # Verify Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        Write-Info "Using Visual Studio: $vsPath"
    } else {
        Write-Warn "vswhere not found - Visual Studio may not be installed correctly."
    }
}

function Set-GnArgs {
    Write-Info "Configuring build ($BuildType)..."
    Push-Location $ChromiumSrc

    $gnArgs = ""

    switch ($BuildType) {
        "debug" {
            $gnArgs = @"
is_debug = true
is_component_build = true
symbol_level = 2
"@
        }
        "release" {
            $gnArgs = @"
is_debug = false
is_official_build = false
is_component_build = false
symbol_level = 1
"@
        }
        "official" {
            $gnArgs = @"
is_debug = false
is_official_build = true
is_component_build = false
symbol_level = 0
"@
        }
    }

    # Windows-specific + Owl-specific args
    $gnArgs += @"

# === Windows Target ===
target_os = "win"
target_cpu = "x64"

# Use local Visual Studio (not Google's internal toolchain)
is_clang = true

# === Owl Browser Configuration ===

# Disable unnecessary features to reduce memory footprint
enable_nacl = false
enable_reading_list = false
enable_vr = false
enable_cardboard = false
enable_arcore = false
enable_openxr = false

# Keep essential features
enable_printing = true
enable_pdf = true
enable_extensions = true
enable_tab_audio_muting = true

# Owl feature flags
owl_memory_optimization = true
owl_aggressive_gc = true
owl_isoheap_enabled = true
owl_arena_allocator = true
owl_adblocker = true
owl_custom_ui = true
owl_memory_pressure_tiers = true
owl_tab_lru_unloading = true
owl_memory_reporter = true

# Build optimizations
use_thin_lto = true
chrome_pgo_phase = 0
"@

    & gn gen $BuildDir --args="$gnArgs"
    if ($LASTEXITCODE -ne 0) { throw "GN gen failed" }

    Write-Info "Build configured at $ChromiumSrc\$BuildDir"
    Pop-Location
}

function Build-Owl {
    Push-Location $ChromiumSrc

    if ($Jobs -le 0) {
        $Jobs = (Get-CimInstance -ClassName Win32_Processor).NumberOfLogicalProcessors
    }

    Write-Info "Building Owl browser with $Jobs parallel jobs..."
    Write-Info "This will take a while (30min - 3h+ depending on hardware)."
    Write-Host ""

    & autoninja -C $BuildDir chrome -j$Jobs
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Pop-Location

    $exePath = Join-Path $ChromiumSrc "$BuildDir\chrome.exe"
    Write-Host ""
    Write-Info "=== Build complete! ==="
    Write-Info "Binary: $exePath"
    Write-Info ""
    Write-Info "Run: & '$exePath'"
}

function Main {
    Write-Info "=== Owl Browser - Build (Windows) ==="
    Write-Info "Build type: $BuildType"
    Write-Info "Build dir:  $BuildDir"
    Write-Host ""

    Test-Environment
    Set-GnArgs
    Build-Owl
}

Main
