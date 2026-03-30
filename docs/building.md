# Building Owl Browser

Owl is a Chromium fork with deep memory management optimizations in the Blink rendering engine.

## Prerequisites

- **Disk space**: 100GB+ free
- **RAM**: 16GB+ recommended (32GB ideal)
- **Compiler**: Clang (only officially supported)
- **Python**: 3.9+
- **OS**: Linux (Ubuntu 20.04+), macOS, or Windows 10+

## Quick Start (Linux)

```bash
# 1. Clone the Owl repository
git clone <owl-repo-url> owl
cd owl

# 2. Fetch Chromium source (~30GB download)
./scripts/fetch_chromium.sh

# 3. Apply Owl patches and copy source files
./scripts/apply_patches.sh

# 4. Build (30min - 2h+ depending on hardware)
./scripts/build.sh

# 5. Run
./chromium/src/out/Owl/chrome
```

## Quick Start (Windows)

**Prerequisites:**
- Windows 10/11 (64-bit)
- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with "Desktop development with C++" workload
- Windows 10/11 SDK (10.0.22621.0+)
- Git and Python 3.9+ in PATH
- 100GB+ free disk, 16GB+ RAM (32GB recommended)

```powershell
# 1. Clone the Owl repository
git clone <owl-repo-url> owl
cd owl

# 2. Fetch Chromium source (~30GB download)
.\scripts\fetch_chromium_win.ps1

# 3. Apply Owl patches and copy source files
.\scripts\apply_patches_win.ps1

# 4. Build (30min - 3h+ depending on hardware)
.\scripts\build_win.ps1

# 5. Run
.\chromium\src\out\Owl\chrome.exe

# 6. (Optional) Generate installer
#    Requires NSIS 3.x: https://nsis.sourceforge.io/
makensis /DBUILD_DIR=chromium\src\out\Owl installer\owl_installer.nsi
# Output: installer\OwlSetup.exe
```

## Build Modes

### Linux
```bash
./scripts/build.sh --debug      # Faster compile, larger binary
./scripts/build.sh --release    # Default, optimized
./scripts/build.sh --official   # Slowest compile, most optimized
```

### Windows
```powershell
.\scripts\build_win.ps1 -BuildType debug
.\scripts\build_win.ps1 -BuildType release    # Default
.\scripts\build_win.ps1 -BuildType official
.\scripts\build_win.ps1 -Jobs 8               # Limit parallel jobs
```

## Custom Build Directory

```bash
BUILD_DIR=out/MyBuild ./scripts/build.sh
```

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| `CHROMIUM_DIR` | `./chromium` | Path to Chromium source checkout |
| `CHROMIUM_VERSION` | `134.0.6998.0` | Chromium version to fetch |
| `BUILD_DIR` | `out/Owl` | Build output directory |
| `BUILD_TYPE` | `release` | Build type (debug/release/official) |
| `OWL_BUILD_JOBS` | `$(nproc)` | Parallel build jobs |

## Feature Flags

All Owl features can be toggled via GN args. See `owl_build/args.gn` for the full list.

To disable a specific feature:

```bash
gn gen out/Owl --args='owl_aggressive_gc=false owl_adblocker=false'
```

## Running Benchmarks

Compare Owl vs Chrome memory usage with 10 tabs:

```bash
./scripts/memory_benchmark.sh
```

Set custom idle time (default 300s):

```bash
IDLE_TIME=600 ./scripts/memory_benchmark.sh
```

## Development Workflow

1. Make changes to files in `src/` or directly in `chromium/src/`
2. Build: `./scripts/build.sh`
3. Test: `./chromium/src/out/Owl/chrome`
4. Generate patches: `./scripts/create_patches.sh`
5. Commit patches to the Owl repository

## Troubleshooting

### Build fails with "out of memory"
Reduce parallel jobs: `./scripts/build.sh --jobs 4`

### Patches fail to apply
The Chromium version may have changed. Update patches:
1. Apply patches to the old version
2. Fetch the new version
3. Resolve conflicts manually
4. Run `./scripts/create_patches.sh` to regenerate

### Missing dependencies on Linux
```bash
cd chromium/src && ./build/install-build-deps.sh
```

### Windows: Visual Studio not found
Install Visual Studio 2022 with the "Desktop development with C++" workload and Windows SDK.

### Windows: generating the installer
Install [NSIS 3.x](https://nsis.sourceforge.io/), then:
```powershell
makensis /DBUILD_DIR=chromium\src\out\Owl installer\owl_installer.nsi
```

### Windows: generating the icon
```powershell
pip install Pillow
python branding\generate_ico.py
```
Replace the generated placeholder with proper artwork before release.
