# Owl Memory Optimizations

Detailed documentation of each memory optimization technique, its origin, and expected impact.

## 1. Oilpan GC Aggressive Collection

**Origin**: Firefox SpiderMonkey + WebKit Riptide
**Files**: `owl_gc_policy.h/cc`, patch `001-oilpan-aggressive-gc.patch`

### What It Does
Customizes Blink's Oilpan garbage collector with three modes:

- **Background mode** (inactive tabs): GC runs every 5 seconds instead of ~30s default. Forces full collection to reclaim maximum memory.
- **Foreground mode** (active tab): Uses concurrent marking inspired by WebKit's Riptide GC. Most marking work runs off the main thread, keeping GC pauses under 10ms.
- **Emergency mode** (memory pressure): Continuous GC with aggressive cache discarding.

### Zombie Compartment Cleanup
When a tab is closed or navigated away, `Persistent<T>` handles pointing to the old page's objects are automatically broken. This prevents the #1 memory leak pattern discovered by Firefox's MemShrink project: long-lived browser objects holding references to short-lived page objects.

### Expected Impact
- 30-50% memory reduction in background tabs
- GC pause times < 10ms (p99)
- Zero memory leaks from closed tabs

---

## 2. PartitionAlloc Tuning

**Origin**: Firefox jemalloc customizations + MemShrink "Clownshoes" audit
**Files**: `owl_partition_config.h`, patch `002-partitionalloc-tuning.patch`

### madvise(MADV_DONTNEED)
After freeing memory, PartitionAlloc calls `madvise(MADV_DONTNEED)` on empty pages. This tells the OS the physical memory can be reclaimed immediately, even though the virtual address space remains mapped. RSS drops immediately instead of waiting for the OS page scanner.

### Aggressive Purge Timer
Inactive renderer processes run periodic memory purges (every 30s) that return empty slot spans to the OS. Active renderers use the default less-frequent purge.

### Allocation Audit
Debug builds log "rounding waste" — cases where an allocation of N bytes gets rounded up to a significantly larger bucket. Firefox's MemShrink project found that a single 1-byte overhead pushed 4KB allocations to 8KB buckets, wasting 700MB on layout-heavy pages.

### Expected Impact
- 10-20% RSS reduction from madvise
- Identification of allocation waste hotspots

---

## 3. Arena Allocator for Layout Data

**Origin**: Firefox nsPresShell arena allocator
**Files**: `owl_arena_allocator.h/cc`

### What It Does
Layout frames, computed styles, and paint records are allocated from a contiguous per-document memory arena instead of individual allocations.

When a page is closed or navigated:
1. The entire arena is freed atomically (single operation)
2. Freed memory is poisoned with a sentinel value to detect use-after-free
3. No individual deallocation overhead

### Benefits
- **Zero fragmentation**: All layout objects are contiguous in memory
- **Better cache locality**: CPU cache lines are fully utilized
- **Instant cleanup**: No GC needed for layout data on page close
- **Security**: Memory poisoning detects UAF bugs

### Expected Impact
- 5-15% improvement in layout-heavy page memory
- Faster page close/navigate (no individual deallocs)

---

## 4. IsoHeap Type Isolation

**Origin**: WebKit bmalloc/libpas
**Files**: patch `005-isoheap-type-isolation.patch`

### What It Does
Security-critical DOM types (Node, Document, EventTarget, Element) are allocated in type-segregated memory spaces. Each type gets its own set of memory pages.

### Benefits
- **Security**: Type confusion attacks cannot exploit adjacent objects of different types
- **Cache locality**: Objects of the same type are contiguous, improving CPU cache hit rates
- **Profiling**: Per-type memory usage is trivially measurable

### Expected Impact
- ~2% Speedometer improvement (cache locality)
- ~8% memory improvement (reduced fragmentation)
- Prevents entire class of type confusion exploits

---

## 5. Frame Freezing

**Origin**: Safari tab freezing + Chrome backgrounding
**Files**: patch `003-frame-memory-budget.patch`

### What It Does
When a tab moves to background:
1. JavaScript timers (setTimeout, setInterval) are paused
2. requestAnimationFrame callbacks are stopped
3. Non-essential network fetches are cancelled
4. Service Workers for the frame are suspended
5. Compositing layers are discarded (recreated on foreground)

### Per-Frame Memory Budget
Each frame has a configurable memory budget. When exceeded:
1. Forced GC on the frame's heap
2. Cache purge (CSS, layout, image caches)
3. If still over budget: frame reported as candidate for tab discard

### Expected Impact
- Background tabs use near-zero CPU
- 20-40% memory reduction per background tab
- Compositing layer discard saves GPU memory

---

## 6. LRU Tab Unloading

**Origin**: Firefox 93 Tab Unloader
**Files**: `owl_tab_discard_policy.h/cc`, patch `004-tab-lifecycle-lru.patch`

### What It Does
Ranks tabs by last interaction time (LRU). Under memory pressure, unloads the least-recently-used tabs first.

### Smart Exceptions
Tabs are protected from unloading if they:
- Are playing audio or video
- Have an active WebRTC session
- Are pinned by the user
- Have unsaved form data
- Were interacted with in the last 5 minutes

### Heavy Tab Detection
When more than 11 tabs are open, the policy estimates per-renderer memory consumption and targets the heaviest tabs for unloading, even if they're not the oldest.

### Expected Impact
- Prevents OOM crashes with many open tabs
- Preserves user workflow (protects active tabs)

---

## 7. 3-Tier Memory Pressure

**Origin**: Safari/iOS Jetsam model
**Files**: `owl_memory_pressure.h/cc`

### Tiers
| Tier | Trigger | Actions |
|---|---|---|
| **Warning** | 80% system RAM | Incremental GC, purge optional caches |
| **Critical** | 90% system RAM | Full GC all threads, freeze background tabs, purge all caches |
| **Emergency** | 95% system RAM | Discard LRU tabs, free discardable memory, keep only active tab |

### Expected Impact
- Proactive memory management before system degrades
- Graceful degradation instead of OOM crash

---

## 8. Ad Blocker with FlatBuffers

**Origin**: Brave adblock engine
**Files**: `owl_request_filter.h/cc`, `owl_filter_list_manager.h/cc`

### What It Does
Network-level ad/tracker blocking using EasyList and EasyPrivacy filter lists.

### FlatBuffers Optimization
Filter lists are compiled into FlatBuffer binary format at update time. The binary is memory-mapped (mmap) at startup with zero deserialization overhead. Brave demonstrated 75% memory reduction with this approach.

### Expected Impact
- Blocks 30-50% of network requests on ad-heavy sites
- Each blocked request saves memory (no DOM, no rendering, no JS execution)
- 75% less memory for filter lists vs text-based parsing

---

## 9. Memory Reporter Framework

**Origin**: Firefox about:memory
**Files**: `owl_memory_monitor.h/cc`, `owl_memory_page.h/cc`, `owl_tabs_page.h/cc`

### Internal Pages
- **owl://memory**: Hierarchical memory breakdown by subsystem (DOM, layout, CSS, JS, images, extensions, GPU). JSON export for automated regression detection.
- **owl://tabs**: Shows LRU priority for each tab, estimated memory per tab, and manual unload buttons.

### Extension Auditing
Monitors memory consumption per installed extension. Reports "memory hog" extensions in the UI so users can make informed decisions.

### Expected Impact
- Full visibility into memory usage
- Enables users to identify memory problems
- Enables automated regression detection in CI
