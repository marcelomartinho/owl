// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/owl_arena_allocator.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "owl_build/owl_buildflags.h"

#if BUILDFLAG(OWL_ARENA_ALLOCATOR)

#include <algorithm>
#include <cstring>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/trace_event/trace_event.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/mman.h>  // madvise
#endif

namespace blink {

// === Chunk Implementation ===

OwlArenaAllocator::Chunk::Chunk(size_t cap)
    : capacity(cap), used(0) {
  // Use mmap for large allocations to enable madvise later.
  // For smaller chunks, use aligned_alloc.
  if (cap >= base::GetPageSize()) {
#if BUILDFLAG(IS_POSIX)
    data = static_cast<uint8_t*>(
        mmap(nullptr, cap, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    CHECK_NE(data, MAP_FAILED) << "OwlArena: mmap failed for " << cap << " bytes";
#else
    data = static_cast<uint8_t*>(malloc(cap));
    CHECK(data) << "OwlArena: malloc failed for " << cap << " bytes";
#endif
  } else {
    data = static_cast<uint8_t*>(malloc(cap));
    CHECK(data) << "OwlArena: malloc failed for " << cap << " bytes";
  }
}

OwlArenaAllocator::Chunk::~Chunk() {
  if (data) {
    if (capacity >= base::GetPageSize()) {
#if BUILDFLAG(IS_POSIX)
      munmap(data, capacity);
#else
      free(data);
#endif
    } else {
      free(data);
    }
    data = nullptr;
  }
}

OwlArenaAllocator::Chunk::Chunk(Chunk&& other) noexcept
    : data(other.data), capacity(other.capacity), used(other.used) {
  other.data = nullptr;
  other.capacity = 0;
  other.used = 0;
}

OwlArenaAllocator::Chunk& OwlArenaAllocator::Chunk::operator=(
    Chunk&& other) noexcept {
  if (this != &other) {
    // Free existing data.
    if (data) {
      if (capacity >= base::GetPageSize()) {
#if BUILDFLAG(IS_POSIX)
        munmap(data, capacity);
#else
        free(data);
#endif
      } else {
        free(data);
      }
    }
    data = other.data;
    capacity = other.capacity;
    used = other.used;
    other.data = nullptr;
    other.capacity = 0;
    other.used = 0;
  }
  return *this;
}

void* OwlArenaAllocator::Chunk::TryAllocate(size_t size, size_t alignment) {
  size_t aligned_offset = (used + alignment - 1) & ~(alignment - 1);
  if (aligned_offset + size > capacity) {
    return nullptr;
  }
  void* ptr = data + aligned_offset;
  used = aligned_offset + size;
  return ptr;
}

void OwlArenaAllocator::Chunk::PoisonAndReset(uint8_t poison_byte) {
  if (data && used > 0) {
    // Poison used memory to detect use-after-free.
    memset(data, poison_byte, used);

    // On POSIX, also tell the OS it can reclaim the physical pages.
    // This immediately reduces RSS (Firefox madvise technique).
#if BUILDFLAG(IS_POSIX)
    if (capacity >= base::GetPageSize()) {
      madvise(data, capacity, MADV_DONTNEED);
    }
#endif
  }
  used = 0;
}

// === OwlArenaAllocator Implementation ===

OwlArenaAllocator::OwlArenaAllocator(size_t chunk_size)
    : chunk_size_(std::max(chunk_size, static_cast<size_t>(4096))) {
}

OwlArenaAllocator::~OwlArenaAllocator() {
  // Destructor frees all chunks automatically via Chunk destructor.
  // No need to poison here since we're destroying.
}

OwlArenaAllocator::OwlArenaAllocator(OwlArenaAllocator&& other) noexcept
    : chunk_size_(other.chunk_size_),
      chunks_(std::move(other.chunks_)),
      stats_(other.stats_) {
  other.stats_ = Stats{};
}

OwlArenaAllocator& OwlArenaAllocator::operator=(
    OwlArenaAllocator&& other) noexcept {
  if (this != &other) {
    chunk_size_ = other.chunk_size_;
    chunks_ = std::move(other.chunks_);
    stats_ = other.stats_;
    other.stats_ = Stats{};
  }
  return *this;
}

void* OwlArenaAllocator::Allocate(size_t size) {
  DCHECK_GT(size, 0u);

  const size_t aligned_size = AlignUp(size, kAlignment);
  stats_.waste_bytes += aligned_size - size;

  // Try to allocate from the current (last) chunk.
  if (!chunks_.empty()) {
    void* ptr = chunks_.back().TryAllocate(aligned_size, kAlignment);
    if (ptr) {
      stats_.total_allocated += size;
      stats_.allocation_count++;
      stats_.peak_allocated =
          std::max(stats_.peak_allocated, stats_.total_allocated);
      return ptr;
    }
  }

  // Current chunk is full (or no chunks exist). Allocate a new one.
  AllocateNewChunk(aligned_size);

  void* ptr = chunks_.back().TryAllocate(aligned_size, kAlignment);
  CHECK(ptr) << "OwlArena: allocation failed after new chunk (size="
             << size << ")";

  stats_.total_allocated += size;
  stats_.allocation_count++;
  stats_.peak_allocated =
      std::max(stats_.peak_allocated, stats_.total_allocated);

  return ptr;
}

void* OwlArenaAllocator::AllocateZeroed(size_t size) {
  void* ptr = Allocate(size);
  memset(ptr, 0, size);
  return ptr;
}

void OwlArenaAllocator::FreeAll() {
  TRACE_EVENT2("blink", "OwlArenaAllocator::FreeAll",
               "chunks", chunks_.size(),
               "total_allocated", stats_.total_allocated);

  // Poison and reset all chunks (but keep the allocations to reuse).
  for (auto& chunk : chunks_) {
    chunk.PoisonAndReset(kPoisonByte);
  }

  // Keep only the first chunk for reuse; free the rest.
  // This prevents unbounded growth if the arena was used for a
  // particularly heavy page.
  if (chunks_.size() > 1) {
    chunks_.resize(1);
  }

  // Reset stats (keep committed count for reporting).
  stats_.total_allocated = 0;
  stats_.allocation_count = 0;
  stats_.waste_bytes = 0;
  // peak_allocated and total_committed are preserved for reporting.
}

bool OwlArenaAllocator::Contains(const void* ptr) const {
  const auto* byte_ptr = static_cast<const uint8_t*>(ptr);
  for (const auto& chunk : chunks_) {
    if (byte_ptr >= chunk.data && byte_ptr < chunk.data + chunk.capacity) {
      return true;
    }
  }
  return false;
}

void OwlArenaAllocator::AllocateNewChunk(size_t min_size) {
  // Allocate at least chunk_size_, but grow if the requested
  // allocation is larger.
  const size_t alloc_size = std::max(chunk_size_, AlignUp(min_size, kAlignment));

  chunks_.emplace_back(alloc_size);
  stats_.total_committed += alloc_size;
  stats_.chunk_count++;
}

}  // namespace blink

#endif  // BUILDFLAG(OWL_ARENA_ALLOCATOR)
