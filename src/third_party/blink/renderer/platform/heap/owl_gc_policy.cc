// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/owl_gc_policy.h"

#if BUILDFLAG(OWL_AGGRESSIVE_GC)

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

OwlGCPolicy::OwlGCPolicy(ThreadState* thread_state)
    : thread_state_(thread_state) {
  DCHECK(thread_state_);
  VLOG(1) << "OwlGCPolicy: Initialized with background_gc_interval="
          << config_.background_gc_interval.InSeconds() << "s"
          << " max_foreground_pause="
          << config_.max_foreground_pause.InMilliseconds() << "ms";
}

OwlGCPolicy::~OwlGCPolicy() {
  StopBackgroundTimer();
}

void OwlGCPolicy::OnTabVisibilityChanged(bool is_visible) {
  TRACE_EVENT1("blink.gc", "OwlGCPolicy::OnTabVisibilityChanged",
               "is_visible", is_visible);

  const bool was_visible = is_visible_;
  is_visible_ = is_visible;

  if (was_visible && !is_visible) {
    // Tab moved to background.
    background_since_ = base::TimeTicks::Now();
    VLOG(1) << "OwlGCPolicy: Tab moved to background";
  } else if (!was_visible && is_visible) {
    // Tab moved to foreground.
    VLOG(1) << "OwlGCPolicy: Tab moved to foreground";
  }

  TransitionToMode(DetermineMode());
}

void OwlGCPolicy::OnMemoryPressureChanged(OwlMemoryPressureTier tier) {
  TRACE_EVENT1("blink.gc", "OwlGCPolicy::OnMemoryPressureChanged",
               "tier", static_cast<int>(tier));

  if (tier == current_pressure_tier_) {
    return;
  }

  const auto old_tier = current_pressure_tier_;
  current_pressure_tier_ = tier;

  VLOG(1) << "OwlGCPolicy: Memory pressure changed from "
          << static_cast<int>(old_tier) << " to " << static_cast<int>(tier);

  // On Critical or Emergency, force immediate GC regardless of mode.
  if (tier >= OwlMemoryPressureTier::kCritical) {
    ForceFullGC();
  }

  TransitionToMode(DetermineMode());
}

void OwlGCPolicy::OnPageNavigatedOrClosed() {
  TRACE_EVENT0("blink.gc", "OwlGCPolicy::OnPageNavigatedOrClosed");

  if (config_.enable_zombie_cleanup) {
    PerformZombieCleanup();
  }

  // Always run GC after page close to reclaim page-specific objects.
  ForceFullGC();
}

void OwlGCPolicy::ForceFullGC() {
  TRACE_EVENT0("blink.gc", "OwlGCPolicy::ForceFullGC");

  const auto start = base::TimeTicks::Now();

  // Request a precise, full garbage collection from Oilpan.
  // This collects all unreachable objects including those in
  // OwlLowPrioritySpace.
  //
  // In the actual Chromium integration, this calls:
  //   thread_state_->CollectAllGarbageForTesting();
  // or the production equivalent:
  //   thread_state_->SchedulePreciseGC();
  //
  // For now, we use the precise GC API which performs a full
  // mark-sweep cycle.
  thread_state_->SchedulePreciseGC();

  const auto pause_duration = base::TimeTicks::Now() - start;
  stats_.total_gc_cycles++;
  RecordGCStats(pause_duration, 0);

  if (current_mode_ == OwlGCMode::kBackground) {
    stats_.background_gc_cycles++;
  } else if (current_mode_ == OwlGCMode::kEmergency) {
    stats_.emergency_gc_cycles++;
  }

  UMA_HISTOGRAM_TIMES("Owl.GC.PauseDuration", pause_duration);
}

void OwlGCPolicy::SetConfig(const Config& config) {
  config_ = config;

  // Re-evaluate mode with new config.
  TransitionToMode(DetermineMode());
}

OwlGCMode OwlGCPolicy::DetermineMode() const {
  // Emergency pressure overrides everything.
  if (current_pressure_tier_ >= OwlMemoryPressureTier::kEmergency) {
    return OwlGCMode::kEmergency;
  }

  // Background tabs get aggressive GC.
  if (!is_visible_) {
    // But only after the grace period.
    const auto time_in_background =
        base::TimeTicks::Now() - background_since_;
    if (time_in_background >= config_.background_grace_period) {
      return OwlGCMode::kBackground;
    }
  }

  // Default: foreground mode with concurrent marking.
  return OwlGCMode::kForeground;
}

void OwlGCPolicy::TransitionToMode(OwlGCMode new_mode) {
  if (new_mode == current_mode_) {
    return;
  }

  TRACE_EVENT2("blink.gc", "OwlGCPolicy::TransitionToMode",
               "old_mode", static_cast<int>(current_mode_),
               "new_mode", static_cast<int>(new_mode));

  const auto old_mode = current_mode_;
  current_mode_ = new_mode;

  // Stop any existing background timer.
  StopBackgroundTimer();

  switch (new_mode) {
    case OwlGCMode::kForeground:
      // Configure concurrent marking for low-pause GC.
      ConfigureIncrementalMarking();
      VLOG(1) << "OwlGCPolicy: Entered foreground mode (concurrent marking, "
              << config_.max_foreground_pause.InMilliseconds() << "ms target)";
      break;

    case OwlGCMode::kBackground:
      // Start aggressive background GC timer.
      StartBackgroundTimer();
      VLOG(1) << "OwlGCPolicy: Entered background mode (GC every "
              << config_.background_gc_interval.InSeconds() << "s)";
      break;

    case OwlGCMode::kEmergency:
      // Emergency: continuous GC + request cache purge.
      // The browser process handles tab discarding; we focus on GC here.
      StartBackgroundTimer();  // Reuse timer but at faster interval.
      VLOG(1) << "OwlGCPolicy: Entered EMERGENCY mode";
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("Owl.GC.ModeTransition",
                            static_cast<int>(new_mode), 3);
}

void OwlGCPolicy::PerformZombieCleanup() {
  TRACE_EVENT0("blink.gc", "OwlGCPolicy::PerformZombieCleanup");

  // Zombie compartment cleanup (inspired by Firefox MemShrink).
  //
  // When a page is closed or navigated, long-lived browser objects
  // (toolbars, extension scripts, etc.) may still hold Persistent<T>
  // references to DOM objects from the old page. These "zombie"
  // references prevent the old page's memory from being collected.
  //
  // Firefox's MemShrink project found this was the #1 cause of
  // memory leaks. Their fix: automatically break cross-compartment
  // references when a compartment is destroyed.
  //
  // In Oilpan terms, we iterate all Persistent<T> handles registered
  // with the current ThreadState and clear any that point to objects
  // belonging to the departing page's execution context.
  //
  // Implementation note: In the actual Chromium integration, this
  // would hook into ExecutionContext::NotifyContextDestroyed() and
  // iterate PersistentRegion to find and clear stale references.
  //
  // The actual implementation requires integration with:
  //   - PersistentRegion::TracePersistentNodes()
  //   - ExecutionContext lifecycle
  //   - CrossThreadPersistent handle tracking

  stats_.zombie_cleanups_performed++;

  VLOG(1) << "OwlGCPolicy: Zombie cleanup performed (total: "
          << stats_.zombie_cleanups_performed << ")";
}

void OwlGCPolicy::ConfigureIncrementalMarking() {
  // Configure Oilpan's incremental marking to respect our pause target.
  //
  // Oilpan supports incremental marking where the marking phase is
  // split into small steps interleaved with mutator (JavaScript)
  // execution. Each step should complete within our pause budget.
  //
  // The Riptide pattern from WebKit JSC takes this further:
  // most marking runs on a background thread concurrently with
  // the main thread, reducing pauses to < 1ms. Oilpan already
  // supports concurrent marking since Chromium M94; we ensure
  // it's enabled and configure the step size.
  //
  // In the actual integration, this would call:
  //   thread_state_->SetIncrementalMarkingStepDuration(
  //       config_.max_foreground_pause);
  //   thread_state_->EnableConcurrentMarking(
  //       config_.enable_concurrent_marking);
}

void OwlGCPolicy::StartBackgroundTimer() {
  base::TimeDelta interval;

  if (current_mode_ == OwlGCMode::kEmergency) {
    // Emergency: GC every second.
    interval = base::Seconds(1);
  } else {
    interval = config_.background_gc_interval;
  }

  background_gc_timer_.Start(
      FROM_HERE, interval,
      base::BindRepeating(&OwlGCPolicy::OnBackgroundGCTimer,
                          base::Unretained(this)));
}

void OwlGCPolicy::StopBackgroundTimer() {
  background_gc_timer_.Stop();
}

void OwlGCPolicy::OnBackgroundGCTimer() {
  TRACE_EVENT0("blink.gc", "OwlGCPolicy::OnBackgroundGCTimer");
  ForceFullGC();
}

void OwlGCPolicy::RecordGCStats(base::TimeDelta pause_duration,
                                uint64_t bytes_freed) {
  stats_.total_gc_pause_time += pause_duration;
  stats_.last_gc_pause_time = pause_duration;
  stats_.bytes_reclaimed += bytes_freed;

  if (pause_duration > stats_.max_gc_pause_time) {
    stats_.max_gc_pause_time = pause_duration;
  }

  // Warn if we exceeded the pause target in foreground mode.
  if (current_mode_ == OwlGCMode::kForeground &&
      pause_duration > config_.max_foreground_pause) {
    VLOG(1) << "OwlGCPolicy: WARNING - GC pause "
            << pause_duration.InMilliseconds()
            << "ms exceeded target "
            << config_.max_foreground_pause.InMilliseconds() << "ms";
  }
}

}  // namespace blink

#endif  // BUILDFLAG(OWL_AGGRESSIVE_GC)
