// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OWL_PARTITION_CONFIG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OWL_PARTITION_CONFIG_H_

// Owl Browser - PartitionAlloc Configuration Profile
//
// This header defines tuned constants for PartitionAlloc to optimize
// memory usage in the Owl browser. These values override Chromium defaults
// when OWL_MEMORY_OPTIMIZATION is enabled.
//
// Key optimizations:
// 1. madvise(MADV_DONTNEED) on freed pages (from Firefox jemalloc)
// 2. Smaller pools and more aggressive purge timers
// 3. Allocation audit logging to find waste (from Firefox MemShrink)
// 4. Lower decommit thresholds to return memory to OS faster
//
// Origins:
// - Firefox jemalloc: madvise integration, arena count tuning
// - Firefox MemShrink "Clownshoes": allocation rounding audit

#include <cstddef>
#include <cstdint>

#if BUILDFLAG(OWL_MEMORY_OPTIMIZATION)

namespace partition_alloc::owl {

// === Pool Configuration ===

// Reduce the super page size to lower per-partition overhead.
// Default Chromium: 2MB. Owl: 1MB.
// Smaller super pages = less wasted address space per partition.
constexpr size_t kOwlSuperPageSize = 1 << 20;  // 1MB

// Reduce the maximum bucketed allocation size.
// Larger allocations bypass the bucket system (go to direct-mapped).
// Default Chromium: ~1MB. Owl: 512KB.
// This reduces the number of bucket sizes and internal fragmentation.
constexpr size_t kOwlMaxBucketedAllocation = 512 * 1024;

// === Purge Configuration ===

// How often inactive renderer processes purge freed memory (seconds).
// Default Chromium: ~120s. Owl: 30s for background, 60s for foreground.
constexpr int kOwlBackgroundPurgeIntervalSeconds = 30;
constexpr int kOwlForegroundPurgeIntervalSeconds = 60;

// Fraction of empty slot spans that triggers a purge.
// Default Chromium: ~0.5 (purge when 50% of slot spans are empty).
// Owl: 0.25 (purge when 25% are empty, more aggressive).
constexpr double kOwlPurgeThreshold = 0.25;

// === madvise Configuration ===

// Enable madvise(MADV_DONTNEED) on freed pages.
// This tells the kernel the physical pages can be reclaimed,
// causing RSS to drop immediately even though virtual address
// space remains mapped. Inspired by Firefox's jemalloc customization.
constexpr bool kOwlUseMadvise = true;

// Minimum number of contiguous freed pages before calling madvise.
// Calling madvise on very small regions has syscall overhead.
constexpr size_t kOwlMadviseMinPages = 2;

// === Decommit Configuration ===

// Enable aggressive decommit of freed pages.
// When a slot span becomes completely empty, decommit its pages
// immediately instead of keeping them in a free pool.
constexpr bool kOwlAggressiveDecommit = true;

// Maximum number of empty slot spans to keep in the free pool
// before force-decommitting. Default Chromium: unlimited.
// Owl: keep at most 4 per partition.
constexpr size_t kOwlMaxEmptySlotSpansPerPartition = 4;

// === Allocation Audit (Debug/Profiling) ===
//
// When enabled, logs allocations where rounding waste exceeds
// a threshold. Firefox's MemShrink project found cases where
// a single extra byte caused allocations to round up to double
// the requested size (the "Clownshoes" bug), wasting hundreds
// of MB on layout-heavy pages.

#if DCHECK_IS_ON()
// Log allocations where waste > 25% of the requested size.
constexpr bool kOwlAllocationAuditEnabled = true;
constexpr double kOwlAllocationAuditWasteThreshold = 0.25;
// Only audit allocations larger than this (ignore small allocs).
constexpr size_t kOwlAllocationAuditMinSize = 256;
#else
constexpr bool kOwlAllocationAuditEnabled = false;
constexpr double kOwlAllocationAuditWasteThreshold = 0.0;
constexpr size_t kOwlAllocationAuditMinSize = 0;
#endif

// === Arena Count (per-platform tuning) ===

// Number of allocation arenas per partition.
// Firefox found that reducing arena count on macOS from the default
// to 4 cut malloc latency by 44% and reduced fragmentation.
// Owl uses a conservative value tuned per platform.
#if defined(OS_MAC)
constexpr size_t kOwlArenaCount = 4;
#elif defined(OS_LINUX)
constexpr size_t kOwlArenaCount = 8;
#else
constexpr size_t kOwlArenaCount = 8;
#endif

// === Reporting ===

// Enable per-partition memory usage reporting for owl://memory.
constexpr bool kOwlPartitionReporting = true;

// Helper to log allocation waste in debug builds.
inline void MaybeLogAllocationWaste(size_t requested, size_t actual) {
  if constexpr (kOwlAllocationAuditEnabled) {
    if (requested >= kOwlAllocationAuditMinSize && actual > requested) {
      const double waste_ratio =
          static_cast<double>(actual - requested) / requested;
      if (waste_ratio > kOwlAllocationAuditWasteThreshold) {
        // In actual integration, this would use VLOG or a histogram:
        // VLOG(2) << "OwlAllocAudit: requested=" << requested
        //         << " actual=" << actual
        //         << " waste=" << (actual - requested)
        //         << " ratio=" << waste_ratio;
      }
    }
  }
}

}  // namespace partition_alloc::owl

#endif  // BUILDFLAG(OWL_MEMORY_OPTIMIZATION)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OWL_PARTITION_CONFIG_H_
