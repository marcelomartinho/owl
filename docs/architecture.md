# Owl Browser Architecture

## Overview

Owl is a Chromium fork focused on memory efficiency and performance. It modifies the Blink rendering engine at the allocator, GC, and frame management levels, and adds browser-level memory management systems.

## Memory Management Stack

```
┌─────────────────────────────────────────────────────┐
│                    Owl Browser UI                     │
│  ┌──────────┐ ┌──────────────┐ ┌─────────────────┐  │
│  │ Tab Strip │ │ Memory Badge │ │ Extension Audit │  │
│  └──────────┘ └──────────────┘ └─────────────────┘  │
├─────────────────────────────────────────────────────┤
│              Browser Process (C++)                    │
│  ┌────────────────┐  ┌───────────────────────────┐  │
│  │  Tab Lifecycle  │  │   Memory Pressure Tiers   │  │
│  │  (LRU Policy)   │  │  Warning → Critical →     │  │
│  │                  │  │  Emergency                │  │
│  └────────────────┘  └───────────────────────────┘  │
│  ┌────────────────┐  ┌───────────────────────────┐  │
│  │  Ad Blocker    │  │   Memory Reporter         │  │
│  │  (FlatBuffers) │  │   Framework               │  │
│  └────────────────┘  └───────────────────────────┘  │
├─────────────────────────────────────────────────────┤
│              Renderer Process (Blink)                 │
│  ┌─────────────────────────────────────────────┐    │
│  │          Oilpan GC (Modified)                │    │
│  │  • Aggressive background collection (5s)     │    │
│  │  • Concurrent marking (Riptide pattern)      │    │
│  │  • Zombie compartment cleanup                │    │
│  │  • IsoHeap type isolation                    │    │
│  │  • <10ms pause target                        │    │
│  └─────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────┐    │
│  │       PartitionAlloc (Tuned)                 │    │
│  │  • madvise(MADV_DONTNEED) on free            │    │
│  │  • Aggressive purge timers                   │    │
│  │  • Allocation audit logging                  │    │
│  └─────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────┐    │
│  │       Arena Allocator (New)                  │    │
│  │  • Per-document layout data arena            │    │
│  │  • Atomic deallocation on page close         │    │
│  │  • Memory poisoning for UAF prevention       │    │
│  └─────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────┐    │
│  │       Frame Management (Modified)            │    │
│  │  • Frame freezing (timers, rAF, SW)          │    │
│  │  • Per-frame memory budget                   │    │
│  │  • Compositing layer discard                 │    │
│  │  • Data structure layout optimization        │    │
│  └─────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

## Techniques by Origin

| Technique | From | Component |
|---|---|---|
| Arena allocator for layout | Firefox nsPresShell | PartitionAlloc layer |
| Zombie compartment cleanup | Firefox MemShrink | Oilpan GC |
| LRU tab unloading | Firefox 93 | Tab Lifecycle |
| madvise(MADV_DONTNEED) | Firefox jemalloc | PartitionAlloc |
| Allocation audit | Firefox MemShrink | PartitionAlloc |
| Memory reporter framework | Firefox about:memory | Memory Monitor |
| IsoHeap type isolation | WebKit bmalloc/libpas | Oilpan GC |
| 3-tier memory pressure | Safari/iOS Jetsam | Memory Pressure |
| Concurrent GC (Riptide) | WebKit JSC | Oilpan GC |
| FlatBuffers filter lists | Brave adblock | Ad Blocker |
| Extension memory audit | Brave | Memory Monitor |
| Data structure layout opt | Chromium 2024 | Frame Management |

## Key Files

### Blink Engine Modifications
- `third_party/blink/renderer/platform/heap/owl_gc_policy.h/cc` - GC policy
- `third_party/blink/renderer/platform/heap/owl_arena_allocator.h/cc` - Arena allocator
- `base/allocator/partition_allocator/owl_partition_config.h` - Alloc config

### Browser Process
- `chrome/browser/owl/owl_tab_discard_policy.h/cc` - Tab lifecycle
- `chrome/browser/owl/owl_memory_pressure.h/cc` - Pressure tiers
- `chrome/browser/owl/owl_memory_monitor.h/cc` - Reporter framework
- `chrome/browser/owl/adblocker/owl_request_filter.h/cc` - Ad blocker

### UI
- `chrome/browser/ui/views/owl/owl_tab_strip.h/cc` - Tab strip + memory
- `chrome/browser/ui/views/owl/owl_memory_indicator.h/cc` - Memory badge

### Internal Pages
- `chrome/browser/owl/pages/owl_memory_page.h/cc` - owl://memory
- `chrome/browser/owl/pages/owl_tabs_page.h/cc` - owl://tabs
