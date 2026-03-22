#!/usr/bin/env bash
# Owl Browser - Fetch Chromium Source
# Downloads and prepares the Chromium source tree for Owl modifications.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OWL_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_DIR="${CHROMIUM_DIR:-$OWL_ROOT/chromium}"
CHROMIUM_VERSION="${CHROMIUM_VERSION:-134.0.6998.0}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[OWL]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[OWL]${NC} $*"; }
log_error() { echo -e "${RED}[OWL]${NC} $*"; }

check_dependencies() {
    local missing=()
    for cmd in git python3 curl; do
        if ! command -v "$cmd" &>/dev/null; then
            missing+=("$cmd")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        exit 1
    fi
}

install_depot_tools() {
    if [ -d "$OWL_ROOT/depot_tools" ]; then
        log_info "depot_tools already installed, updating..."
        cd "$OWL_ROOT/depot_tools" && git pull --quiet
        return
    fi

    log_info "Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git \
        "$OWL_ROOT/depot_tools"
}

fetch_chromium_source() {
    export PATH="$OWL_ROOT/depot_tools:$PATH"

    if [ -d "$CHROMIUM_DIR/src" ]; then
        log_warn "Chromium source already exists at $CHROMIUM_DIR/src"
        log_info "To re-fetch, remove $CHROMIUM_DIR and run again."
        return
    fi

    log_info "Fetching Chromium source (this will take a while, ~30GB)..."
    mkdir -p "$CHROMIUM_DIR"
    cd "$CHROMIUM_DIR"

    cat > .gclient <<EOF
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git@$CHROMIUM_VERSION",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_nacl": False,
      "checkout_telemetry_dependencies": False,
    },
  },
]
EOF

    log_info "Running gclient sync (downloading source + dependencies)..."
    gclient sync --nohooks --no-history -j"$(nproc)"

    log_info "Running hooks..."
    cd src
    gclient runhooks
}

install_build_deps() {
    if [ "$(uname)" = "Linux" ]; then
        log_info "Installing Linux build dependencies..."
        cd "$CHROMIUM_DIR/src"
        ./build/install-build-deps.sh --no-prompt
    else
        log_warn "Non-Linux platform detected. Install build deps manually."
        log_warn "See: https://chromium.googlesource.com/chromium/src/+/main/docs/"
    fi
}

main() {
    log_info "=== Owl Browser - Chromium Source Fetch ==="
    log_info "Target version: $CHROMIUM_VERSION"
    log_info "Target directory: $CHROMIUM_DIR"
    echo ""

    check_dependencies
    install_depot_tools
    fetch_chromium_source
    install_build_deps

    log_info ""
    log_info "=== Chromium source ready! ==="
    log_info "Next steps:"
    log_info "  1. ./scripts/apply_patches.sh  # Apply Owl patches"
    log_info "  2. ./scripts/build.sh           # Build Owl browser"
}

main "$@"
