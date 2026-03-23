// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_GC_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_GC_POLICY_H_

#include <cstdint>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "owl_build/owl_buildflags.h"
#include "third_party/blink/renderer/platform/platform_export.h"

#if BUILDFLAG(OWL_AGGRESSIVE_GC)

namespace blink {

class ThreadState;

// Owl GC collection modes, determined by tab visibility and system memory.
enum class OwlGCMode {
  // Active (foreground) tab: normal GC with concurrent marking.
  // Marking runs off main thread (Riptide pattern from WebKit JSC).
  // Pause target: <10ms.
  kForeground,

  // Background tab: aggressive GC every 5 seconds.
  // Full mark-sweep to reclaim maximum memory.
  kBackground,

  // System memory pressure >= 95%: continuous GC + cache discard.
  // Only reached when OwlMemoryPressure reports Emergency tier.
  kEmergency,
};

// Memory pressure tiers inspired by Safari/iOS Jetsam model.
// These tiers drive the GC policy and tab lifecycle decisions.
enum class OwlMemoryPressureTier {
  kNone,       // < 80% system RAM used
  kWarning,    // 80-90%: incremental GC + purge optional caches
  kCritical,   // 90-95%: full GC all threads + freeze background tabs
  kEmergency,  // > 95%: discard LRU tabs + free discardable memory
};

// OwlGCPolicy manages garbage collection behavior based on tab state
// and system memory pressure. It hooks into Blink's ThreadState to
// override default GC scheduling.
//
// Key optimizations:
// - Background tabs: GC every 5s (vs ~30s default Chromium)
// - Foreground tabs: Concurrent marking with <10ms pause target
// - Emergency mode: Continuous GC when system memory is critically low
// - Zombie cleanup: Breaks Persistent<T> references on tab close/navigate
//
// Origins:
// - Aggressive background GC: Firefox MemShrink project
// - Concurrent marking: WebKit Riptide GC
// - Memory pressure tiers: Safari/iOS Jetsam model
// - Zombie cleanup: Firefox zombie compartment fix
class PLATFORM_EXPORT OwlGCPolicy {
 public:
  // Configuration for GC timing and thresholds.
  struct Config {
    // How often to run GC in background tabs.
    base::TimeDelta background_gc_interval = base::Seconds(5);

    // Maximum GC pause time for foreground tabs.
    base::TimeDelta max_foreground_pause = base::Milliseconds(10);

    // Memory pressure thresholds (fraction of total system RAM).
    double warning_threshold = 0.80;
    double critical_threshold = 0.90;
    double emergency_threshold = 0.95;

    // Minimum time a tab must be in background before aggressive GC.
    base::TimeDelta background_grace_period = base::Seconds(30);

    // Enable zombie compartment cleanup on navigation/close.
    bool enable_zombie_cleanup = true;

    // Enable concurrent marking in foreground mode.
    bool enable_concurrent_marking = true;
  };

  explicit OwlGCPolicy(ThreadState* thread_state);
  ~OwlGCPolicy();

  OwlGCPolicy(const OwlGCPolicy&) = delete;
  OwlGCPolicy& operator=(const OwlGCPolicy&) = delete;

  // Called when the tab's visibility changes.
  void OnTabVisibilityChanged(bool is_visible);

  // Called when the system memory pressure tier changes.
  // This is triggered by OwlMemoryPressure in the browser process
  // and forwarded to renderer via IPC.
  void OnMemoryPressureChanged(OwlMemoryPressureTier tier);

  // Called when a page is about to be navigated away or closed.
  // Triggers zombie compartment cleanup: breaks Persistent<T> references
  // from long-lived browser objects to short-lived page objects.
  // This prevents the #1 memory leak pattern found by Firefox MemShrink.
  void OnPageNavigatedOrClosed();

  // Forces an immediate full GC cycle. Used by emergency mode
  // and the owl://memory diagnostic page.
  void ForceFullGC();

  // Returns the current GC mode.
  OwlGCMode current_mode() const { return current_mode_; }

  // Returns the current memory pressure tier.
  OwlMemoryPressureTier current_pressure_tier() const {
    return current_pressure_tier_;
  }

  // Returns true if the tab is currently in background.
  bool is_background() const { return !is_visible_; }

  // Updates the config. Can be called at runtime.
  void SetConfig(const Config& config);
  const Config& config() const { return config_; }

  // Statistics for owl://memory reporting.
  struct Stats {
    uint64_t total_gc_cycles = 0;
    uint64_t background_gc_cycles = 0;
    uint64_t emergency_gc_cycles = 0;
    uint64_t zombie_cleanups_performed = 0;
    base::TimeDelta total_gc_pause_time;
    base::TimeDelta max_gc_pause_time;
    base::TimeDelta last_gc_pause_time;
    uint64_t objects_reclaimed = 0;
    uint64_t bytes_reclaimed = 0;
  };

  const Stats& stats() const { return stats_; }

 private:
  // Timer callback for background GC scheduling.
  void OnBackgroundGCTimer();

  // Determines the appropriate GC mode based on visibility and pressure.
  OwlGCMode DetermineMode() const;

  // Transitions to a new GC mode, adjusting timers and thresholds.
  void TransitionToMode(OwlGCMode new_mode);

  // Performs zombie compartment cleanup.
  // Iterates Persistent<T> handles and breaks references to objects
  // belonging to the closing/navigating page's heap.
  void PerformZombieCleanup();

  // Configures Oilpan's incremental marking to respect the pause target.
  void ConfigureIncrementalMarking();

  // Starts/stops the background GC timer.
  void StartBackgroundTimer();
  void StopBackgroundTimer();

  // Records GC statistics after a collection.
  void RecordGCStats(base::TimeDelta pause_duration, uint64_t bytes_freed);

  ThreadState* thread_state_;  // Not owned.
  Config config_;
  OwlGCMode current_mode_ = OwlGCMode::kForeground;
  OwlMemoryPressureTier current_pressure_tier_ =
      OwlMemoryPressureTier::kNone;
  bool is_visible_ = true;
  base::TimeTicks background_since_;
  base::RepeatingTimer background_gc_timer_;
  Stats stats_;
};

}  // namespace blink

#endif  // BUILDFLAG(OWL_AGGRESSIVE_GC)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OWL_GC_POLICY_H_
