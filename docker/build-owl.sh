#!/usr/bin/env bash
# Owl Browser - Docker Build Script
# Runs inside the Docker container to fetch, patch, and build Chromium with Owl modifications.
set -euo pipefail

BUILD_ROOT="/build"
OWL_SRC="/owl"
CHROMIUM_DIR="$BUILD_ROOT/chromium"
CHROMIUM_SRC="$CHROMIUM_DIR/src"
DEPOT_TOOLS="/opt/depot_tools"
BUILD_OUTPUT="out/Owl"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[OWL]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[OWL]${NC} $*"; }
log_error() { echo -e "${RED}[OWL]${NC} $*"; }
log_step()  { echo -e "${CYAN}[OWL]${NC} === $* ==="; }

export PATH="$DEPOT_TOOLS:$PATH"

# ============================================================
# STEP 1: Fetch Chromium Source
# ============================================================
fetch_chromium() {
    log_step "Step 1: Fetching Chromium Source"

    if [ -d "$CHROMIUM_SRC/.gn" ]; then
        log_info "Chromium source already exists at $CHROMIUM_SRC"
        log_info "Skipping fetch. Delete $CHROMIUM_DIR to re-fetch."
        return
    fi

    # Get latest stable version
    log_info "Determining latest stable Chromium version..."
    local version
    version=$(curl -s "https://chromiumdash.appspot.com/fetch/milestones" | \
        python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    for m in data:
        if m.get('chromium_main_branch_position'):
            print(m.get('chromium_branch', 'unknown'))
            sys.exit(0)
    print('unknown')
except:
    print('unknown')
" 2>/dev/null || echo "unknown")

    # Try versionhistory API as fallback
    if [ "$version" = "unknown" ]; then
        version=$(curl -s "https://versionhistory.googleapis.com/v1/chrome/platforms/linux/channels/stable/versions" | \
            python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    print(data['versions'][0]['version'])
except:
    print('unknown')
" 2>/dev/null || echo "unknown")
    fi

    # Hard fallback to known stable version
    if [ "$version" = "unknown" ]; then
        version="134.0.6998.88"
        log_warn "Could not determine latest stable version, using known stable: $version"
    fi

    log_info "Target version: $version"
    log_info "This will download ~30GB. Be patient..."

    mkdir -p "$CHROMIUM_DIR"
    cd "$CHROMIUM_DIR"

    # Configure gclient
    cat > .gclient <<EOF
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git@$version",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      "checkout_nacl": False,
      "checkout_telemetry_dependencies": False,
    },
  },
]
EOF

    log_info "Running gclient sync (this takes 1-3 hours)..."
    gclient sync --nohooks --no-history -j"$(nproc)"

    log_info "Running hooks..."
    cd "$CHROMIUM_SRC"
    gclient runhooks

    log_info "Installing build dependencies..."
    ./build/install-build-deps.sh --no-prompt || true

    log_info "Chromium source fetched successfully."
}

# ============================================================
# STEP 2: Copy Owl Sources into Chromium Tree
# ============================================================
copy_owl_sources() {
    log_step "Step 2: Copying Owl Sources"

    local src_dirs=(
        "third_party/blink/renderer/platform/heap"
        "base/allocator/partition_allocator"
        "chrome/browser/owl"
        "chrome/browser/owl/adblocker"
        "chrome/browser/owl/pages"
        "chrome/browser/ui/views/owl"
    )

    for dir in "${src_dirs[@]}"; do
        local owl_dir="$OWL_SRC/src/$dir"
        local chromium_dest="$CHROMIUM_SRC/$dir"

        if [ -d "$owl_dir" ] && [ "$(ls -A "$owl_dir" 2>/dev/null)" ]; then
            mkdir -p "$chromium_dest"
            # Only copy owl_* files to avoid overwriting Chromium files
            cp -v "$owl_dir"/owl_* "$chromium_dest"/ 2>/dev/null || true
            # Copy BUILD.gn only if it exists in owl source
            if [ -f "$owl_dir/BUILD.gn" ]; then
                # For owl-specific dirs, copy BUILD.gn directly
                if [[ "$dir" == *"owl"* ]]; then
                    cp -v "$owl_dir/BUILD.gn" "$chromium_dest/BUILD.gn"
                fi
            fi
            log_info "  Copied: $dir"
        fi
    done

    # Copy owl_build config
    mkdir -p "$CHROMIUM_SRC/owl_build"
    cp -v "$OWL_SRC/owl_build"/* "$CHROMIUM_SRC/owl_build/"
    log_info "  Copied: owl_build/"

    # Copy pages directory if exists
    if [ -d "$OWL_SRC/src/chrome/browser/owl/pages" ]; then
        mkdir -p "$CHROMIUM_SRC/chrome/browser/owl/pages"
        cp -v "$OWL_SRC/src/chrome/browser/owl/pages"/* \
            "$CHROMIUM_SRC/chrome/browser/owl/pages/" 2>/dev/null || true
        log_info "  Copied: chrome/browser/owl/pages/"
    fi
}

# ============================================================
# STEP 3: Create Integration Patches
# ============================================================
create_integration_patches() {
    log_step "Step 3: Creating Integration Patches"
    cd "$CHROMIUM_SRC"

    # --- Patch 1: Hook owl into chrome/browser/BUILD.gn ---
    log_info "Patch 1: Adding owl dep to chrome/browser/BUILD.gn..."
    local browser_build="chrome/browser/BUILD.gn"
    if [ -f "$browser_build" ]; then
        # Clean previous owl patches first (idempotent)
        sed -i '/owl_features\.gni/d' "$browser_build"
        sed -i '/\/\/chrome\/browser\/owl"/d' "$browser_build"
        # Apply fresh
        sed -i '1s|^|import("//owl_build/owl_features.gni")\n|' "$browser_build"
        # Match only standalone "deps = [" (not public_deps)
        sed -i '0,/^  deps = \[/{s|^  deps = \[|  deps = [\n    "//chrome/browser/owl",|}' "$browser_build"
        log_info "  OK: chrome/browser/BUILD.gn patched"
    else
        log_warn "  chrome/browser/BUILD.gn not found, will need manual patching"
    fi

    # --- Patch 2: Hook owl_ui into chrome/browser/ui/views/BUILD.gn ---
    log_info "Patch 2: Adding owl_ui dep to chrome/browser/ui/views/BUILD.gn..."
    local views_build="chrome/browser/ui/views/BUILD.gn"
    if [ -f "$views_build" ]; then
        # Clean previous owl patches first (idempotent)
        sed -i '/owl_features\.gni/d' "$views_build"
        sed -i '/chrome\/browser\/ui\/views\/owl/d' "$views_build"
        # Apply fresh
        sed -i '1s|^|import("//owl_build/owl_features.gni")\n|' "$views_build"
        # Match only standalone "deps = [" (not public_deps), add owl_ui dep
        sed -i '0,/^  deps = \[/{s|^  deps = \[|  deps = [\n    "//chrome/browser/ui/views/owl:owl_ui",|}' "$views_build"
        log_info "  OK: chrome/browser/ui/views/BUILD.gn patched"
    else
        log_warn "  chrome/browser/ui/views/BUILD.gn not found"
    fi

    # --- Patch 3: Hook Blink heap owl files ---
    log_info "Patch 3: Adding owl files to Blink heap BUILD.gn..."
    local heap_build="third_party/blink/renderer/platform/heap/BUILD.gn"
    if [ -f "$heap_build" ]; then
        # Clean previous owl patches (idempotent)
        sed -i '/owl_features\.gni/d' "$heap_build"
        sed -i '/^# Owl Browser - GC policy/,$ d' "$heap_build"
        # Apply fresh import
        sed -i '1s|^|import("//owl_build/owl_features.gni")\n|' "$heap_build"

        # Append the owl targets
        cat >> "$heap_build" <<'PATCH_EOF'

# Owl Browser - GC policy and arena allocator
# These targets live inside the heap directory so they can access
# Blink heap headers via relative includes without needing to
# depend on the visibility-restricted :heap target.
if (owl_aggressive_gc) {
  source_set("owl_gc") {
    sources = [
      "owl_gc_policy.cc",
      "owl_gc_policy.h",
    ]
    deps = [
      "//base",
      "//owl_build:owl_buildflags",
    ]
    # Include paths needed to resolve forward-declared ThreadState
    include_dirs = [
      "//third_party/blink/renderer",
    ]
    visibility = [ "*" ]
  }
}

if (owl_arena_allocator) {
  source_set("owl_arena") {
    sources = [
      "owl_arena_allocator.cc",
      "owl_arena_allocator.h",
    ]
    deps = [
      "//base",
      "//owl_build:owl_buildflags",
    ]
    visibility = [ "*" ]
  }
}
PATCH_EOF
        log_info "  OK: Blink heap BUILD.gn patched"
    else
        log_warn "  Blink heap BUILD.gn not found"
    fi

    # --- Patch 4: Add owl_buildflags dep to owl BUILD.gn ---
    log_info "Patch 4: Ensuring owl BUILD.gn has owl_buildflags dep..."
    local owl_build="chrome/browser/owl/BUILD.gn"
    if [ -f "$owl_build" ] && ! grep -q "owl_buildflags" "$owl_build"; then
        sed -i 's|deps = \[|deps = [\n    "//owl_build:owl_buildflags",|' "$owl_build"
        log_info "  OK: owl BUILD.gn patched"
    fi

    # --- Patch 5: Add owl_buildflags dep to owl_ui BUILD.gn ---
    local owl_ui_build="chrome/browser/ui/views/owl/BUILD.gn"
    if [ -f "$owl_ui_build" ] && ! grep -q "owl_buildflags" "$owl_ui_build"; then
        sed -i 's|deps = \[|deps = [\n    "//owl_build:owl_buildflags",|' "$owl_ui_build"
        log_info "  OK: owl_ui BUILD.gn patched"
    fi

    log_info "Integration patches applied."
}

# ============================================================
# STEP 4: Configure and Build
# ============================================================
configure_and_build() {
    log_step "Step 4: Configure and Build"
    cd "$CHROMIUM_SRC"

    local build_type="${BUILD_TYPE:-debug}"
    local jobs="${OWL_BUILD_JOBS:-$(nproc)}"

    log_info "Build type: $build_type"
    log_info "Parallel jobs: $jobs"

    local gn_args=""
    case "$build_type" in
        debug)
            gn_args="is_debug=true is_component_build=true symbol_level=2"
            ;;
        release)
            gn_args="is_debug=false is_component_build=false symbol_level=1"
            ;;
    esac

    # Add common Owl args
    gn_args+="
enable_nacl=false
enable_reading_list=false
enable_vr=false
enable_tab_audio_muting=true

owl_memory_optimization=true
owl_aggressive_gc=true
owl_isoheap_enabled=true
owl_arena_allocator=true
owl_adblocker=true
owl_custom_ui=true
owl_memory_pressure_tiers=true
owl_tab_lru_unloading=true
owl_memory_reporter=true
"

    # Ensure depot_tools is bootstrapped (needed when running in a fresh container)
    log_info "Bootstrapping depot_tools..."
    if [ -f "$DEPOT_TOOLS/ensure_bootstrap" ]; then
        "$DEPOT_TOOLS/ensure_bootstrap"
    fi
    # Also run gclient runhooks to ensure all tools are available
    if [ ! -f "$DEPOT_TOOLS/python3_bin_reldir.txt" ]; then
        log_info "Initializing depot_tools via gclient..."
        gclient --version 2>/dev/null || true
        # Try update_depot_tools as fallback
        if [ -f "$DEPOT_TOOLS/update_depot_tools" ]; then
            "$DEPOT_TOOLS/update_depot_tools" || true
        fi
    fi

    log_info "Running gn gen..."
    gn gen "$BUILD_OUTPUT" --args="$gn_args"

    log_info "Building Owl browser (this takes 30min - 2h+)..."
    autoninja -C "$BUILD_OUTPUT" chrome -j"$jobs"

    log_info ""
    log_info "Build complete!"
    log_info "Binary: $CHROMIUM_SRC/$BUILD_OUTPUT/chrome"
}

# ============================================================
# MAIN
# ============================================================
main() {
    log_step "Owl Browser - Full Build Pipeline"
    log_info "Build root: $BUILD_ROOT"
    log_info "Owl source: $OWL_SRC"
    echo ""

    local step="${1:-all}"

    case "$step" in
        fetch)
            fetch_chromium
            ;;
        copy)
            copy_owl_sources
            ;;
        patch)
            create_integration_patches
            ;;
        build)
            configure_and_build
            ;;
        all)
            fetch_chromium
            copy_owl_sources
            create_integration_patches
            configure_and_build
            ;;
        *)
            echo "Usage: $0 [fetch|copy|patch|build|all]"
            echo ""
            echo "Steps:"
            echo "  fetch  - Download Chromium source (~30GB, 1-3h)"
            echo "  copy   - Copy Owl sources into Chromium tree"
            echo "  patch  - Apply integration patches to BUILD.gn files"
            echo "  build  - Configure GN and compile"
            echo "  all    - Run all steps (default)"
            exit 1
            ;;
    esac
}

main "$@"
