#!/usr/bin/env bash
# Owl Browser - Build Script
# Configures GN args and compiles the Owl browser.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OWL_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="${CHROMIUM_DIR:-$OWL_ROOT/chromium}/src"
BUILD_DIR="${BUILD_DIR:-out/Owl}"
BUILD_TYPE="${BUILD_TYPE:-release}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[OWL]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[OWL]${NC} $*"; }
log_error() { echo -e "${RED}[OWL]${NC} $*"; }

check_environment() {
    export PATH="$OWL_ROOT/depot_tools:$PATH"

    if [ ! -d "$CHROMIUM_SRC" ]; then
        log_error "Chromium source not found at $CHROMIUM_SRC"
        log_error "Run ./scripts/fetch_chromium.sh first."
        exit 1
    fi

    if ! command -v gn &>/dev/null; then
        log_error "GN not found. Ensure depot_tools is in PATH."
        exit 1
    fi
}

configure_build() {
    log_info "Configuring build ($BUILD_TYPE)..."
    cd "$CHROMIUM_SRC"

    local gn_args=""

    case "$BUILD_TYPE" in
        debug)
            gn_args="
is_debug = true
is_component_build = true
symbol_level = 2
"
            ;;
        release)
            gn_args="
is_debug = false
is_official_build = false
is_component_build = false
symbol_level = 1
"
            ;;
        official)
            gn_args="
is_debug = false
is_official_build = true
is_component_build = false
symbol_level = 0
"
            ;;
    esac

    # Append Owl-specific args
    gn_args+="
# === Owl Browser Configuration ===

# Disable unnecessary features to reduce memory footprint
enable_nacl = false
enable_reading_list = false

# Keep tab audio muting - used by memory policy
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

# Build optimizations
use_thin_lto = true
"

    gn gen "$BUILD_DIR" --args="$gn_args"
    log_info "Build configured at $CHROMIUM_SRC/$BUILD_DIR"
}

build_owl() {
    cd "$CHROMIUM_SRC"
    local jobs="${OWL_BUILD_JOBS:-$(nproc)}"

    log_info "Building Owl browser with $jobs parallel jobs..."
    log_info "This will take a while (30min - 2h+ depending on hardware)."
    echo ""

    autoninja -C "$BUILD_DIR" chrome -j"$jobs"

    log_info ""
    log_info "=== Build complete! ==="
    log_info "Binary: $CHROMIUM_SRC/$BUILD_DIR/chrome"
}

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --debug      Build in debug mode (faster build, larger binary)"
    echo "  --release    Build in release mode (default)"
    echo "  --official   Build in official mode (slowest, most optimized)"
    echo "  --jobs N     Set number of parallel build jobs"
    echo "  --dir DIR    Set build output directory (default: out/Owl)"
    echo "  -h, --help   Show this help message"
}

main() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --debug)    BUILD_TYPE="debug"; shift ;;
            --release)  BUILD_TYPE="release"; shift ;;
            --official) BUILD_TYPE="official"; shift ;;
            --jobs)     export OWL_BUILD_JOBS="$2"; shift 2 ;;
            --dir)      BUILD_DIR="$2"; shift 2 ;;
            -h|--help)  show_usage; exit 0 ;;
            *)          log_error "Unknown option: $1"; show_usage; exit 1 ;;
        esac
    done

    log_info "=== Owl Browser - Build ==="
    log_info "Build type: $BUILD_TYPE"
    log_info "Build dir: $BUILD_DIR"
    echo ""

    check_environment
    configure_build
    build_owl
}

main "$@"
