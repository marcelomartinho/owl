# Owl Browser

A memory-optimized Chromium fork with deep modifications to the Blink rendering engine.

## Goal

**30-40% less memory** than vanilla Chrome, with full Chrome Web Store extension support.

## Key Optimizations

| Optimization | Origin | Impact |
|---|---|---|
| Aggressive Oilpan GC (background: 5s cycle) | Firefox MemShrink + WebKit Riptide | 30-50% less memory in background tabs |
| Arena allocator for layout data | Firefox nsPresShell | Zero fragmentation, atomic cleanup |
| IsoHeap type isolation | WebKit bmalloc/libpas | +2% perf, +8% memory, type confusion prevention |
| madvise(MADV_DONTNEED) on free | Firefox jemalloc | Immediate RSS reduction |
| LRU tab unloading | Firefox 93 | Prevents OOM with many tabs |
| 3-tier memory pressure | Safari/iOS Jetsam | Proactive degradation vs crash |
| FlatBuffers ad blocker | Brave | 75% less memory for filter lists |
| Frame freezing | Safari | Near-zero CPU for background tabs |
| Zombie compartment cleanup | Firefox MemShrink | Zero leaks from closed tabs |

## Chrome Web Store Extensions

Full support. Owl is a direct Chromium fork, so all Manifest V2 and V3 extensions work natively.

## Building

```bash
# Fetch Chromium source (~30GB)
./scripts/fetch_chromium.sh

# Apply Owl patches
./scripts/apply_patches.sh

# Build
./scripts/build.sh

# Run
./chromium/src/out/Owl/chrome
```

See [docs/building.md](docs/building.md) for detailed instructions.

## Architecture

See [docs/architecture.md](docs/architecture.md) for the full memory management stack diagram.

## Memory Optimizations

See [docs/memory-optimizations.md](docs/memory-optimizations.md) for detailed documentation of each technique.

## Internal Pages

- `owl://memory` - Hierarchical memory breakdown by subsystem
- `owl://tabs` - Tab LRU priority, per-tab memory, manual unload

## Benchmarks

```bash
./scripts/memory_benchmark.sh
```

Compares Owl vs Chrome with 10 real-world sites open for 5 minutes.

## Repository Structure

```
owl/
├── patches/          # Patches applied to Chromium source
├── src/              # New Owl source files (copied into Chromium tree)
├── owl_build/        # GN build configuration and feature flags
├── scripts/          # Build automation and benchmarks
└── docs/             # Architecture and optimization documentation
```

## License

Owl is based on Chromium and follows the same BSD-style license.
