# Owl Browser - Complete Windows Setup & Build
# Single script that runs all steps: fetch, patch, build, and generate installer.
#
# Usage:
#   .\scripts\setup_and_build.ps1                  # Full build (default: release)
#   .\scripts\setup_and_build.ps1 -BuildType debug  # Debug build
#   .\scripts\setup_and_build.ps1 -SkipFetch        # Skip Chromium download (if already fetched)
#   .\scripts\setup_and_build.ps1 -SkipBuild        # Only fetch + patch, don't build
#
# Requirements:
#   - Windows 10/11 (64-bit)
#   - Visual Studio 2022 with "Desktop development with C++" workload
#   - Windows 10/11 SDK (10.0.22621.0+)
#   - Git and Python 3.9+ in PATH
#   - NSIS 3.x (for installer generation): https://nsis.sourceforge.io/
#   - 100GB+ free disk, 16GB+ RAM (32GB recommended)
#   - Internet connection (~30GB download)

param(
    [ValidateSet("debug", "release", "official")]
    [string]$BuildType = "release",

    [int]$Jobs = 0,

    [switch]$SkipFetch,
    [switch]$SkipBuild,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OwlRoot = Split-Path -Parent $ScriptDir
$ChromiumDir = Join-Path $OwlRoot "chromium"
$ChromiumSrc = Join-Path $ChromiumDir "src"
$BuildDir = "out\Owl"
$ChromiumVersion = "134.0.6998.0"

$TotalSteps = 4
$StepTimings = @{}

# ============================================================
# Helpers
# ============================================================

function Write-Banner { param($msg)
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step { param($step, $msg)
    Write-Host "[Step $step/$TotalSteps] $msg" -ForegroundColor Green
}

function Write-Info  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Green }
function Write-Warn  { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Yellow }
function Write-Err   { param($msg) Write-Host "[OWL] $msg" -ForegroundColor Red }

function Get-ElapsedTime { param($sw)
    $elapsed = $sw.Elapsed
    if ($elapsed.TotalHours -ge 1) {
        return "{0:N0}h {1:N0}m {2:N0}s" -f $elapsed.Hours, $elapsed.Minutes, $elapsed.Seconds
    } elseif ($elapsed.TotalMinutes -ge 1) {
        return "{0:N0}m {1:N0}s" -f $elapsed.Minutes, $elapsed.Seconds
    } else {
        return "{0:N1}s" -f $elapsed.TotalSeconds
    }
}

function Assert-Command { param($cmd, $installHint)
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Err "'$cmd' not found in PATH."
        if ($installHint) { Write-Err $installHint }
        exit 1
    }
}

# ============================================================
# Pre-flight checks
# ============================================================

function Test-Prerequisites {
    Write-Banner "Pre-flight Checks"

    # OS check
    if ($env:OS -ne "Windows_NT") {
        Write-Err "This script must run on Windows."
        Write-Err "For Linux, use: ./scripts/fetch_chromium.sh && ./scripts/apply_patches.sh && ./scripts/build.sh"
        exit 1
    }

    # Git
    Assert-Command "git" "Install Git: https://git-scm.com/download/win"
    Write-Info "Git: OK"

    # Python
    $pythonCmd = if (Get-Command "python" -ErrorAction SilentlyContinue) { "python" }
                 elseif (Get-Command "python3" -ErrorAction SilentlyContinue) { "python3" }
                 else { $null }
    if (-not $pythonCmd) {
        Write-Err "Python not found. Install Python 3.9+: https://www.python.org/downloads/"
        exit 1
    }
    $pyVer = & $pythonCmd --version 2>&1
    Write-Info "Python: $pyVer"

    # Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        Write-Info "Visual Studio: $vsPath"
    } else {
        Write-Err "Visual Studio 2022 not found!"
        Write-Err "Download: https://visualstudio.microsoft.com/downloads/"
        Write-Err "Install with 'Desktop development with C++' workload."
        exit 1
    }

    # Disk space
    $drive = (Get-Item $OwlRoot).PSDrive
    $freeGB = [math]::Round((Get-PSDrive $drive.Name).Free / 1GB, 1)
    if ($freeGB -lt 100) {
        Write-Warn "Only ${freeGB}GB free disk space. Recommended: 100GB+."
        Write-Warn "Build may fail if disk runs out."
    } else {
        Write-Info "Disk space: ${freeGB}GB free"
    }

    # RAM
    $totalRAM = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
    if ($totalRAM -lt 16) {
        Write-Warn "Only ${totalRAM}GB RAM. Recommended: 16GB+ (32GB ideal)."
    } else {
        Write-Info "RAM: ${totalRAM}GB"
    }

    # NSIS (optional, for installer)
    if (-not $SkipInstaller) {
        if (Get-Command "makensis" -ErrorAction SilentlyContinue) {
            Write-Info "NSIS: OK"
        } else {
            $nsisPath = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
            if (Test-Path $nsisPath) {
                $env:PATH += ";${env:ProgramFiles(x86)}\NSIS"
                Write-Info "NSIS: $nsisPath"
            } else {
                Write-Warn "NSIS not found. Installer step will be skipped."
                Write-Warn "Install from: https://nsis.sourceforge.io/"
                $script:SkipInstaller = $true
            }
        }
    }

    Write-Info "All checks passed!"
}

# ============================================================
# Step 1: Fetch Chromium
# ============================================================

function Invoke-FetchChromium {
    Write-Step 1 "Fetching Chromium source ($ChromiumVersion)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    # depot_tools
    $depotToolsDir = Join-Path $OwlRoot "depot_tools"
    if (Test-Path $depotToolsDir) {
        Write-Info "depot_tools exists, updating..."
        Push-Location $depotToolsDir
        git pull --quiet
        Pop-Location
    } else {
        Write-Info "Cloning depot_tools..."
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $depotToolsDir
    }
    $env:PATH = "$depotToolsDir;$env:PATH"
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

    # Chromium source
    if (Test-Path (Join-Path $ChromiumSrc ".gn")) {
        Write-Info "Chromium source already exists. Skipping download."
    } else {
        Write-Info "Downloading Chromium source (~30GB). This will take a while..."
        New-Item -ItemType Directory -Force -Path $ChromiumDir | Out-Null
        Push-Location $ChromiumDir

        $gclient = @"
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
        Set-Content -Path ".gclient" -Value $gclient

        $cpuCount = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
        & gclient sync --nohooks --no-history -j$cpuCount
        if ($LASTEXITCODE -ne 0) { throw "gclient sync failed" }

        Write-Info "Running hooks..."
        Push-Location "src"
        & gclient runhooks
        if ($LASTEXITCODE -ne 0) { throw "gclient runhooks failed" }
        Pop-Location
        Pop-Location
    }

    $sw.Stop()
    $StepTimings["fetch"] = Get-ElapsedTime $sw
    Write-Info "Step 1 complete ($(Get-ElapsedTime $sw))"
}

# ============================================================
# Step 2: Apply Patches
# ============================================================

function Invoke-ApplyPatches {
    Write-Step 2 "Applying Owl patches to Chromium"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    if (-not (Test-Path $ChromiumSrc)) {
        Write-Err "Chromium source not found. Step 1 may have failed."
        exit 1
    }

    # Copy Owl source files
    Write-Info "Copying Owl source files..."
    $srcDirs = @(
        "third_party\blink\renderer\platform\heap",
        "base\allocator\partition_allocator",
        "chrome\browser\owl",
        "chrome\browser\owl\adblocker",
        "chrome\browser\owl\pages",
        "chrome\browser\ui\views\owl"
    )

    $copied = 0
    foreach ($dir in $srcDirs) {
        $owlSrc = Join-Path $OwlRoot "src\$dir"
        $dest = Join-Path $ChromiumSrc $dir
        if ((Test-Path $owlSrc) -and (Get-ChildItem $owlSrc -ErrorAction SilentlyContinue)) {
            New-Item -ItemType Directory -Force -Path $dest | Out-Null
            Copy-Item -Path "$owlSrc\*" -Destination $dest -Force
            $fileCount = (Get-ChildItem $owlSrc -File).Count
            $copied += $fileCount
            Write-Info "  $dir ($fileCount files)"
        }
    }
    Write-Info "Copied $copied source files."

    # Apply .patch files
    $patchDir = Join-Path $OwlRoot "patches"
    if ((Test-Path $patchDir) -and (Get-ChildItem "$patchDir\*.patch" -ErrorAction SilentlyContinue)) {
        Push-Location $ChromiumSrc
        $applied = 0; $failed = 0
        foreach ($patch in Get-ChildItem "$patchDir\*.patch") {
            & git apply --check $patch.FullName 2>&1 | Out-Null
            if ($LASTEXITCODE -eq 0) {
                & git apply $patch.FullName
                $applied++
                Write-Info "  Patch OK: $($patch.Name)"
            } else {
                & git apply --3way $patch.FullName 2>&1 | Out-Null
                if ($LASTEXITCODE -eq 0) {
                    $applied++
                    Write-Warn "  Patch 3-way: $($patch.Name)"
                } else {
                    $failed++
                    Write-Err "  Patch FAILED: $($patch.Name)"
                }
            }
        }
        Write-Info "Patches: $applied applied, $failed failed"
        Pop-Location
    } else {
        Write-Info "No .patch files found (source files copied directly)."
    }

    # Copy build configs
    $owlBuild = Join-Path $OwlRoot "owl_build"
    if (Test-Path $owlBuild) {
        $dest = Join-Path $ChromiumSrc "owl_build"
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Copy-Item -Path "$owlBuild\*" -Destination $dest -Force
        Write-Info "Copied build configs."
    }

    $sw.Stop()
    $StepTimings["patch"] = Get-ElapsedTime $sw
    Write-Info "Step 2 complete ($(Get-ElapsedTime $sw))"
}

# ============================================================
# Step 3: Build
# ============================================================

function Invoke-Build {
    Write-Step 3 "Building Owl Browser ($BuildType)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    $depotToolsDir = Join-Path $OwlRoot "depot_tools"
    $env:PATH = "$depotToolsDir;$env:PATH"
    $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

    Push-Location $ChromiumSrc

    # Configure GN args
    $gnArgs = switch ($BuildType) {
        "debug"    { "is_debug = true`nis_component_build = true`nsymbol_level = 2" }
        "release"  { "is_debug = false`nis_official_build = false`nis_component_build = false`nsymbol_level = 1" }
        "official" { "is_debug = false`nis_official_build = true`nis_component_build = false`nsymbol_level = 0" }
    }

    $gnArgs += @"

target_os = "win"
target_cpu = "x64"
is_clang = true
enable_nacl = false
enable_reading_list = false
enable_vr = false
enable_cardboard = false
enable_arcore = false
enable_openxr = false
enable_printing = true
enable_pdf = true
enable_extensions = true
enable_tab_audio_muting = true
owl_memory_optimization = true
owl_aggressive_gc = true
owl_isoheap_enabled = true
owl_arena_allocator = true
owl_adblocker = true
owl_custom_ui = true
owl_memory_pressure_tiers = true
owl_tab_lru_unloading = true
owl_memory_reporter = true
use_thin_lto = true
chrome_pgo_phase = 0
"@

    Write-Info "Running GN gen..."
    & gn gen $BuildDir --args="$gnArgs"
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "GN gen failed" }

    # Build
    if ($Jobs -le 0) {
        $Jobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
    }
    Write-Info "Compiling with $Jobs parallel jobs..."
    Write-Info "This will take 30min - 3h+ depending on your hardware."
    Write-Host ""

    & autoninja -C $BuildDir chrome -j$Jobs
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "Build failed" }

    Pop-Location

    $exePath = Join-Path $ChromiumSrc "$BuildDir\chrome.exe"
    if (Test-Path $exePath) {
        $size = [math]::Round((Get-Item $exePath).Length / 1MB, 1)
        Write-Info "Binary ready: $exePath (${size}MB)"
    }

    $sw.Stop()
    $StepTimings["build"] = Get-ElapsedTime $sw
    Write-Info "Step 3 complete ($(Get-ElapsedTime $sw))"
}

# ============================================================
# Step 4: Generate Installer
# ============================================================

function Invoke-Installer {
    Write-Step 4 "Generating OwlSetup.exe installer"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    # Generate icon if missing
    $icoPath = Join-Path $OwlRoot "branding\owl.ico"
    if (-not (Test-Path $icoPath)) {
        Write-Info "Generating placeholder icon..."
        $genScript = Join-Path $OwlRoot "branding\generate_ico.py"

        # Install Pillow if needed
        & python -c "import PIL" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Info "Installing Pillow..."
            & pip install Pillow --quiet
        }

        & python $genScript
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Icon generation failed. Creating minimal placeholder..."
            # Create a minimal valid .ico (1x1 pixel) as fallback
            $bytes = [byte[]]@(0,0,1,0,1,0,1,1,0,0,1,0,32,0,68,0,0,0,22,0,0,0,40,0,0,0,1,0,0,0,2,0,0,0,1,0,32,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,45,55,72,255,0,0,0,0)
            [System.IO.File]::WriteAllBytes($icoPath, $bytes)
        }
    }
    Write-Info "Icon: $icoPath"

    # Create LICENSE if missing (NSIS expects it)
    $licensePath = Join-Path $OwlRoot "LICENSE"
    if (-not (Test-Path $licensePath)) {
        Set-Content -Path $licensePath -Value @"
Owl Browser - BSD 3-Clause License

Copyright (c) 2026, Owl Browser Project
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.

This software is based on Chromium, which is licensed under the BSD license.
See https://chromium.googlesource.com/chromium/src/+/main/LICENSE
"@
        Write-Info "Created LICENSE file."
    }

    # Run NSIS
    $nsiScript = Join-Path $OwlRoot "installer\owl_installer.nsi"
    $buildOutput = Join-Path $ChromiumSrc $BuildDir

    Write-Info "Running makensis..."
    Push-Location (Join-Path $OwlRoot "installer")
    & makensis /DBUILD_DIR="$buildOutput" $nsiScript
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "NSIS installer generation failed" }
    Pop-Location

    $installerPath = Join-Path $OwlRoot "installer\OwlSetup.exe"
    if (Test-Path $installerPath) {
        $size = [math]::Round((Get-Item $installerPath).Length / 1MB, 1)
        Write-Info "Installer ready: $installerPath (${size}MB)"
    }

    $sw.Stop()
    $StepTimings["installer"] = Get-ElapsedTime $sw
    Write-Info "Step 4 complete ($(Get-ElapsedTime $sw))"
}

# ============================================================
# Main
# ============================================================

function Main {
    $totalSw = [System.Diagnostics.Stopwatch]::StartNew()

    Write-Banner "Owl Browser - Windows Setup & Build"
    Write-Host "  Build type:  $BuildType"
    Write-Host "  Skip fetch:  $SkipFetch"
    Write-Host "  Skip build:  $SkipBuild"
    Write-Host "  Skip installer: $SkipInstaller"
    Write-Host ""

    Test-Prerequisites

    # Step 1
    if (-not $SkipFetch) {
        Invoke-FetchChromium
    } else {
        Write-Step 1 "SKIPPED (fetch)"
    }

    # Step 2
    Invoke-ApplyPatches

    # Step 3
    if (-not $SkipBuild) {
        Invoke-Build
    } else {
        Write-Step 3 "SKIPPED (build)"
    }

    # Step 4
    if (-not $SkipInstaller) {
        Invoke-Installer
    } else {
        Write-Step 4 "SKIPPED (installer)"
    }

    $totalSw.Stop()

    # Summary
    Write-Banner "Build Complete!"
    Write-Host "  Timings:" -ForegroundColor Cyan
    foreach ($key in @("fetch", "patch", "build", "installer")) {
        if ($StepTimings.ContainsKey($key)) {
            Write-Host "    $($key.PadRight(12)) $($StepTimings[$key])" -ForegroundColor White
        }
    }
    Write-Host "    $("TOTAL".PadRight(12)) $(Get-ElapsedTime $totalSw)" -ForegroundColor Yellow
    Write-Host ""

    $exePath = Join-Path $ChromiumSrc "$BuildDir\chrome.exe"
    $installerPath = Join-Path $OwlRoot "installer\OwlSetup.exe"

    if ((-not $SkipBuild) -and (Test-Path $exePath)) {
        Write-Host "  Run directly:" -ForegroundColor Cyan
        Write-Host "    & '$exePath'" -ForegroundColor White
        Write-Host ""
    }
    if ((-not $SkipInstaller) -and (Test-Path $installerPath)) {
        Write-Host "  Install:" -ForegroundColor Cyan
        Write-Host "    & '$installerPath'" -ForegroundColor White
        Write-Host ""
    }

    Write-Info "Done! Enjoy Owl Browser."
}

try {
    Main
} catch {
    Write-Err "Build failed: $_"
    Write-Err $_.ScriptStackTrace
    exit 1
}
