#!/usr/bin/env bash
# Owl Browser - Create Patches from Chromium Modifications
# Generates patch files from changes made to the Chromium source tree.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OWL_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="${CHROMIUM_DIR:-$OWL_ROOT/chromium}/src"
PATCH_DIR="$OWL_ROOT/patches"

GREEN='\033[0;32m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[OWL]${NC} $*"; }

create_patch() {
    local name="$1"
    shift
    local paths=("$@")
    local patch_file="$PATCH_DIR/$name.patch"

    cd "$CHROMIUM_SRC"

    log_info "Creating patch: $name"
    git diff -- "${paths[@]}" > "$patch_file"

    if [ -s "$patch_file" ]; then
        log_info "  Created: $patch_file ($(wc -l < "$patch_file") lines)"
    else
        rm -f "$patch_file"
        log_info "  No changes found for: $name"
    fi
}

main() {
    log_info "=== Owl Browser - Create Patches ==="
    mkdir -p "$PATCH_DIR"

    # Patch 001: Oilpan GC modifications
    create_patch "001-oilpan-aggressive-gc" \
        "third_party/blink/renderer/platform/heap/thread_state.h" \
        "third_party/blink/renderer/platform/heap/thread_state.cc" \
        "third_party/blink/renderer/platform/heap/heap_allocator_impl.h" \
        "third_party/blink/renderer/platform/heap/custom_spaces.h" \
        "third_party/blink/renderer/platform/heap/custom_spaces.cc"

    # Patch 002: PartitionAlloc tuning
    create_patch "002-partitionalloc-tuning" \
        "base/allocator/partition_allocator/partition_alloc_constants.h" \
        "base/allocator/partition_allocator/partition_alloc.h" \
        "base/allocator/partition_allocator/partition_alloc.cc"

    # Patch 003: Frame memory management
    create_patch "003-frame-memory-budget" \
        "third_party/blink/renderer/core/frame/local_frame.cc" \
        "third_party/blink/renderer/core/frame/local_frame.h" \
        "third_party/blink/renderer/core/frame/local_frame_view.cc" \
        "third_party/blink/renderer/core/frame/local_frame_view.h"

    # Patch 004: Tab lifecycle
    create_patch "004-tab-lifecycle-lru" \
        "chrome/browser/resource_coordinator/" \
        "chrome/browser/ui/tabs/"

    # Patch 005: IsoHeap type isolation
    create_patch "005-isoheap-type-isolation" \
        "third_party/blink/renderer/platform/heap/custom_spaces.h" \
        "third_party/blink/renderer/platform/heap/custom_spaces.cc" \
        "third_party/blink/renderer/core/dom/node.h" \
        "third_party/blink/renderer/core/dom/document.h" \
        "third_party/blink/renderer/core/dom/element.h"

    # Patch 006: Disable unused features
    create_patch "006-disable-unused-features" \
        "chrome/browser/BUILD.gn" \
        "chrome/browser/ui/BUILD.gn"

    log_info ""
    log_info "=== Patches created in $PATCH_DIR ==="
}

main "$@"
