#!/usr/bin/env bash
# Owl Browser - Apply Patches to Chromium Source
# Applies Owl modification patches and copies new source files.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OWL_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="${CHROMIUM_DIR:-$OWL_ROOT/chromium}/src"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[OWL]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[OWL]${NC} $*"; }
log_error() { echo -e "${RED}[OWL]${NC} $*"; }

check_chromium_src() {
    if [ ! -d "$CHROMIUM_SRC" ]; then
        log_error "Chromium source not found at $CHROMIUM_SRC"
        log_error "Run ./scripts/fetch_chromium.sh first."
        exit 1
    fi
}

copy_owl_sources() {
    log_info "Copying Owl source files into Chromium tree..."

    local src_dirs=(
        "third_party/blink/renderer/platform/heap"
        "base/allocator/partition_allocator"
        "chrome/browser/owl"
        "chrome/browser/owl/adblocker"
        "chrome/browser/owl/pages"
        "chrome/browser/ui/views/owl"
    )

    for dir in "${src_dirs[@]}"; do
        local owl_src="$OWL_ROOT/src/$dir"
        local chromium_dest="$CHROMIUM_SRC/$dir"

        if [ -d "$owl_src" ] && [ "$(ls -A "$owl_src" 2>/dev/null)" ]; then
            mkdir -p "$chromium_dest"
            cp -v "$owl_src"/* "$chromium_dest"/ 2>/dev/null || true
            log_info "  Copied: $dir"
        fi
    done
}

apply_patches() {
    log_info "Applying Owl patches to Chromium source..."
    cd "$CHROMIUM_SRC"

    local patch_dir="$OWL_ROOT/patches"
    local applied=0
    local failed=0

    if [ ! -d "$patch_dir" ] || [ -z "$(ls "$patch_dir"/*.patch 2>/dev/null)" ]; then
        log_warn "No patches found in $patch_dir"
        return
    fi

    for patch in "$patch_dir"/*.patch; do
        local patch_name="$(basename "$patch")"
        log_info "  Applying: $patch_name"

        if git apply --check "$patch" 2>/dev/null; then
            git apply "$patch"
            ((applied++))
            log_info "    OK: $patch_name"
        else
            # Try with 3-way merge for better conflict resolution
            if git apply --3way "$patch" 2>/dev/null; then
                ((applied++))
                log_warn "    Applied with 3-way merge: $patch_name"
            else
                ((failed++))
                log_error "    FAILED: $patch_name"
                log_error "    Manual resolution may be required."
            fi
        fi
    done

    echo ""
    log_info "Patches applied: $applied, Failed: $failed"
}

update_build_files() {
    log_info "Updating GN build configuration..."

    # Copy Owl build configs
    if [ -d "$OWL_ROOT/owl_build" ]; then
        mkdir -p "$CHROMIUM_SRC/owl_build"
        cp -v "$OWL_ROOT/owl_build"/* "$CHROMIUM_SRC/owl_build"/ 2>/dev/null || true
        log_info "  Copied owl_build configs"
    fi
}

main() {
    log_info "=== Owl Browser - Apply Patches ==="
    echo ""

    check_chromium_src
    copy_owl_sources
    apply_patches
    update_build_files

    log_info ""
    log_info "=== Patches applied successfully! ==="
    log_info "Next step: ./scripts/build.sh"
}

main "$@"
