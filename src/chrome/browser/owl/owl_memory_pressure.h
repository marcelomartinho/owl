// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_OWL_MEMORY_PRESSURE_H_
#define CHROME_BROWSER_OWL_OWL_MEMORY_PRESSURE_H_

#include <cstdint>

#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"

namespace owl {

// 3-tier memory pressure response system inspired by Safari/iOS Jetsam.
//
// Tiers:
//   None     (< 80% RAM): No action
//   Warning  (80-90%): Incremental GC + purge optional caches
//   Critical (90-95%): Full GC all threads + freeze background tabs
//   Emergency (> 95%): Discard LRU tabs, free discardable memory
//
// This is proactive: we act BEFORE the OS starts killing processes.
// The system memory monitor runs every 2 seconds.
enum class MemoryPressureTier {
  kNone = 0,
  kWarning = 1,
  kCritical = 2,
  kEmergency = 3,
};

class OwlMemoryPressureObserver {
 public:
  virtual ~OwlMemoryPressureObserver() = default;
  virtual void OnOwlMemoryPressureChanged(MemoryPressureTier tier) = 0;
};

class OwlMemoryPressure {
 public:
  struct Config {
    double warning_threshold = 0.80;
    double critical_threshold = 0.90;
    double emergency_threshold = 0.95;
    base::TimeDelta poll_interval = base::Seconds(2);
  };

  explicit OwlMemoryPressure(const Config& config = Config{});
  ~OwlMemoryPressure();

  OwlMemoryPressure(const OwlMemoryPressure&) = delete;
  OwlMemoryPressure& operator=(const OwlMemoryPressure&) = delete;

  void Start();
  void Stop();

  void AddObserver(OwlMemoryPressureObserver* observer);
  void RemoveObserver(OwlMemoryPressureObserver* observer);

  MemoryPressureTier current_tier() const { return current_tier_; }
  double current_memory_usage_fraction() const { return current_usage_; }

  // For testing: manually set the memory usage fraction.
  void SetMemoryUsageForTesting(double usage_fraction);

  struct Stats {
    uint64_t tier_transitions = 0;
    uint64_t warning_events = 0;
    uint64_t critical_events = 0;
    uint64_t emergency_events = 0;
    base::TimeDelta time_in_warning;
    base::TimeDelta time_in_critical;
    base::TimeDelta time_in_emergency;
  };

  const Stats& stats() const { return stats_; }

 private:
  void OnPollTimer();
  double GetSystemMemoryUsage() const;
  MemoryPressureTier DetermineTier(double usage) const;
  void TransitionToTier(MemoryPressureTier new_tier);

  // Actions for each tier.
  void HandleWarning();
  void HandleCritical();
  void HandleEmergency();

  Config config_;
  MemoryPressureTier current_tier_ = MemoryPressureTier::kNone;
  double current_usage_ = 0.0;
  base::TimeTicks tier_entered_at_;
  base::RepeatingTimer poll_timer_;
  base::ObserverList<OwlMemoryPressureObserver> observers_;
  Stats stats_;

  // For testing.
  bool use_test_usage_ = false;
  double test_usage_ = 0.0;
};

}  // namespace owl

#endif  // CHROME_BROWSER_OWL_OWL_MEMORY_PRESSURE_H_
