// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_ARENA_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_ARENA_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/check.h"
#include "third_party/blink/renderer/platform/platform_export.h"

#if BUILDFLAG(OWL_ARENA_ALLOCATOR)

namespace blink {

// OwlArenaAllocator - Per-document arena allocator for layout data.
//
// Inspired by Firefox's nsPresShell arena allocator, this provides
// bump-pointer allocation from contiguous memory chunks for layout-related
// objects (layout frames, computed styles, paint records).
//
// Key benefits:
// 1. Zero fragmentation: all objects are contiguous in memory
// 2. Excellent cache locality: sequential access patterns hit cache
// 3. Atomic deallocation: entire arena freed at once when page closes
//    (no individual free() calls, no GC needed for layout data)
// 4. Security: freed memory is poisoned to detect use-after-free
//
// Usage:
//   OwlArenaAllocator arena;
//   void* mem = arena.Allocate(sizeof(LayoutBox));
//   // ... use mem ...
//   // When page closes:
//   arena.FreeAll();  // Frees everything at once, poisons memory
//
// This allocator is NOT thread-safe. Each Document gets its own arena
// used only on the main thread.
//
// Origin: Firefox nsPresShell::AllocateFrame() / nsPresContext::AllocateFromShell()
class PLATFORM_EXPORT OwlArenaAllocator {
 public:
  // Default chunk size: 64KB. Large enough to hold many layout objects
  // without excessive mmap calls, small enough to not waste memory
  // for simple pages.
  static constexpr size_t kDefaultChunkSize = 64 * 1024;

  // Alignment for all allocations (matches CPU cache line on most platforms).
  static constexpr size_t kAlignment = 16;

  // Poison value written to freed memory to detect use-after-free.
  // Using 0xCD (same as MSVC debug heap) for easy recognition.
  static constexpr uint8_t kPoisonByte = 0xCD;

  explicit OwlArenaAllocator(size_t chunk_size = kDefaultChunkSize);
  ~OwlArenaAllocator();

  OwlArenaAllocator(const OwlArenaAllocator&) = delete;
  OwlArenaAllocator& operator=(const OwlArenaAllocator&) = delete;

  // Move is allowed (for container storage).
  OwlArenaAllocator(OwlArenaAllocator&& other) noexcept;
  OwlArenaAllocator& operator=(OwlArenaAllocator&& other) noexcept;

  // Allocate `size` bytes from the arena.
  // Returns aligned pointer. Never returns nullptr (crashes on OOM).
  // The returned memory is NOT zero-initialized for performance.
  void* Allocate(size_t size);

  // Allocate and zero-initialize.
  void* AllocateZeroed(size_t size);

  // Free ALL memory in the arena at once.
  // All previously returned pointers become invalid.
  // Memory is poisoned to detect use-after-free.
  void FreeAll();

  // Returns true if the pointer was allocated from this arena.
  bool Contains(const void* ptr) const;

  // Statistics for owl://memory reporting.
  struct Stats {
    size_t total_allocated = 0;      // Bytes allocated by user
    size_t total_committed = 0;      // Bytes committed from OS
    size_t chunk_count = 0;          // Number of chunks allocated
    size_t peak_allocated = 0;       // Peak user allocation
    size_t allocation_count = 0;     // Number of Allocate() calls
    size_t waste_bytes = 0;          // Alignment waste
  };

  const Stats& stats() const { return stats_; }

  // Returns the overhead ratio: committed / allocated.
  // Values close to 1.0 mean low waste.
  double overhead_ratio() const {
    if (stats_.total_allocated == 0) return 0.0;
    return static_cast<double>(stats_.total_committed) /
           stats_.total_allocated;
  }

 private:
  // A contiguous chunk of memory from which we bump-allocate.
  struct Chunk {
    uint8_t* data;       // Pointer to allocated memory
    size_t capacity;     // Total size of the chunk
    size_t used;         // Bytes used so far (bump pointer offset)

    Chunk(size_t cap);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&& other) noexcept;
    Chunk& operator=(Chunk&& other) noexcept;

    // Returns aligned pointer, or nullptr if not enough space.
    void* TryAllocate(size_t size, size_t alignment);

    // Poison all memory in this chunk and reset.
    void PoisonAndReset(uint8_t poison_byte);
  };

  // Allocate a new chunk, at least `min_size` bytes.
  void AllocateNewChunk(size_t min_size);

  // Round up to alignment.
  static size_t AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  size_t chunk_size_;
  std::vector<Chunk> chunks_;
  Stats stats_;
};

// Typed arena allocation helper.
// Usage:
//   auto* box = OwlArenaNew<LayoutBox>(arena, constructor_args...);
template <typename T, typename... Args>
T* OwlArenaNew(OwlArenaAllocator& arena, Args&&... args) {
  void* mem = arena.Allocate(sizeof(T));
  return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace blink

#endif  // BUILDFLAG(OWL_ARENA_ALLOCATOR)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_ARENA_ALLOCATOR_H_
