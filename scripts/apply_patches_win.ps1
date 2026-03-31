# Owl Browser - Apply Patches to Chromium Source (Windows)
# Applies Owl modification patches and copies new source files.
param(
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

function Test-ChromiumSrc {
    if (-not (Test-Path $ChromiumSrc)) {
        Write-Err "Chromium source not found at $ChromiumSrc"
        Write-Err "Run .\scripts\fetch_chromium_win.ps1 first."
        exit 1
    }
}

function Copy-OwlSources {
    Write-Info "Copying Owl source files into Chromium tree..."

    $srcDirs = @(
        "third_party\blink\renderer\platform\heap",
        "base\allocator\partition_allocator",
        "chrome\browser\owl",
        "chrome\browser\owl\adblocker",
        "chrome\browser\owl\pages",
        "chrome\browser\ui\views\owl"
    )

    foreach ($dir in $srcDirs) {
        $owlSrc = Join-Path $OwlRoot "src\$dir"
        $chromiumDest = Join-Path $ChromiumSrc $dir

        if ((Test-Path $owlSrc) -and (Get-ChildItem $owlSrc -ErrorAction SilentlyContinue)) {
            New-Item -ItemType Directory -Force -Path $chromiumDest | Out-Null
            Copy-Item -Path "$owlSrc\*" -Destination $chromiumDest -Force
            Write-Info "  Copied: $dir"
        }
    }
}

function Apply-Patches {
    Write-Info "Applying Owl patches to Chromium source..."
    Push-Location $ChromiumSrc

    $patchDir = Join-Path $OwlRoot "patches"
    $applied = 0
    $failed = 0

    if (-not (Test-Path $patchDir) -or -not (Get-ChildItem "$patchDir\*.patch" -ErrorAction SilentlyContinue)) {
        Write-Warn "No patches found in $patchDir"
        Pop-Location
        return
    }

    foreach ($patch in Get-ChildItem "$patchDir\*.patch") {
        Write-Info "  Applying: $($patch.Name)"

        $check = & git apply --check $patch.FullName 2>&1
        if ($LASTEXITCODE -eq 0) {
            & git apply $patch.FullName
            $applied++
            Write-Info "    OK: $($patch.Name)"
        } else {
            $threeWay = & git apply --3way $patch.FullName 2>&1
            if ($LASTEXITCODE -eq 0) {
                $applied++
                Write-Warn "    Applied with 3-way merge: $($patch.Name)"
            } else {
                $failed++
                Write-Err "    FAILED: $($patch.Name)"
                Write-Err "    Manual resolution may be required."
            }
        }
    }

    Write-Host ""
    Write-Info "Patches applied: $applied, Failed: $failed"
    Pop-Location
}

function Update-BuildFiles {
    Write-Info "Updating GN build configuration..."

    $owlBuild = Join-Path $OwlRoot "owl_build"
    if (Test-Path $owlBuild) {
        $dest = Join-Path $ChromiumSrc "owl_build"
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Copy-Item -Path "$owlBuild\*" -Destination $dest -Force
        Write-Info "  Copied owl_build configs"
    }
}

function Main {
    Write-Info "=== Owl Browser - Apply Patches (Windows) ==="
    Write-Host ""

    Test-ChromiumSrc
    Copy-OwlSources
    Apply-Patches
    Update-BuildFiles

    Write-Host ""
    Write-Info "=== Patches applied successfully! ==="
    Write-Info "Next step: .\scripts\build_win.ps1"
}

Main
