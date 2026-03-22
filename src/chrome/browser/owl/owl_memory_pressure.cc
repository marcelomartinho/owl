// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/owl_memory_pressure.h"

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"

namespace owl {

OwlMemoryPressure::OwlMemoryPressure(const Config& config)
    : config_(config) {}

OwlMemoryPressure::~OwlMemoryPressure() {
  Stop();
}

void OwlMemoryPressure::Start() {
  VLOG(1) << "OwlMemoryPressure: Starting monitor (poll every "
          << config_.poll_interval.InSeconds() << "s)";
  tier_entered_at_ = base::TimeTicks::Now();
  poll_timer_.Start(FROM_HERE, config_.poll_interval,
                    base::BindRepeating(&OwlMemoryPressure::OnPollTimer,
                                        base::Unretained(this)));
}

void OwlMemoryPressure::Stop() {
  poll_timer_.Stop();
}

void OwlMemoryPressure::AddObserver(OwlMemoryPressureObserver* observer) {
  observers_.AddObserver(observer);
}

void OwlMemoryPressure::RemoveObserver(OwlMemoryPressureObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OwlMemoryPressure::SetMemoryUsageForTesting(double usage_fraction) {
  use_test_usage_ = true;
  test_usage_ = usage_fraction;
}

void OwlMemoryPressure::OnPollTimer() {
  current_usage_ = GetSystemMemoryUsage();
  const auto new_tier = DetermineTier(current_usage_);

  if (new_tier != current_tier_) {
    TransitionToTier(new_tier);
  }
}

double OwlMemoryPressure::GetSystemMemoryUsage() const {
  if (use_test_usage_) {
    return test_usage_;
  }

  // Get total and available system memory.
  const int64_t total_memory = base::SysInfo::AmountOfPhysicalMemory();
  const int64_t available_memory =
      base::SysInfo::AmountOfAvailablePhysicalMemory();

  if (total_memory <= 0) {
    return 0.0;
  }

  const int64_t used_memory = total_memory - available_memory;
  return static_cast<double>(used_memory) / total_memory;
}

MemoryPressureTier OwlMemoryPressure::DetermineTier(double usage) const {
  if (usage >= config_.emergency_threshold) {
    return MemoryPressureTier::kEmergency;
  }
  if (usage >= config_.critical_threshold) {
    return MemoryPressureTier::kCritical;
  }
  if (usage >= config_.warning_threshold) {
    return MemoryPressureTier::kWarning;
  }
  return MemoryPressureTier::kNone;
}

void OwlMemoryPressure::TransitionToTier(MemoryPressureTier new_tier) {
  TRACE_EVENT2("browser", "OwlMemoryPressure::TransitionToTier",
               "old_tier", static_cast<int>(current_tier_),
               "new_tier", static_cast<int>(new_tier));

  // Record time spent in the previous tier.
  const auto now = base::TimeTicks::Now();
  const auto duration = now - tier_entered_at_;

  switch (current_tier_) {
    case MemoryPressureTier::kWarning:
      stats_.time_in_warning += duration;
      break;
    case MemoryPressureTier::kCritical:
      stats_.time_in_critical += duration;
      break;
    case MemoryPressureTier::kEmergency:
      stats_.time_in_emergency += duration;
      break;
    default:
      break;
  }

  current_tier_ = new_tier;
  tier_entered_at_ = now;
  stats_.tier_transitions++;

  VLOG(1) << "OwlMemoryPressure: Tier changed to "
          << static_cast<int>(new_tier)
          << " (usage: " << static_cast<int>(current_usage_ * 100) << "%)";

  // Execute tier-specific actions.
  switch (new_tier) {
    case MemoryPressureTier::kWarning:
      stats_.warning_events++;
      HandleWarning();
      break;
    case MemoryPressureTier::kCritical:
      stats_.critical_events++;
      HandleCritical();
      break;
    case MemoryPressureTier::kEmergency:
      stats_.emergency_events++;
      HandleEmergency();
      break;
    case MemoryPressureTier::kNone:
      break;
  }

  // Notify observers (tab lifecycle, GC policy, etc.).
  for (auto& observer : observers_) {
    observer.OnOwlMemoryPressureChanged(new_tier);
  }
}

void OwlMemoryPressure::HandleWarning() {
  VLOG(1) << "OwlMemoryPressure: WARNING tier - "
          << "requesting incremental GC + cache purge";

  // Signal Chromium's built-in memory pressure system.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

void OwlMemoryPressure::HandleCritical() {
  VLOG(1) << "OwlMemoryPressure: CRITICAL tier - "
          << "requesting full GC + tab freeze";

  // Signal critical pressure to Chromium.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

void OwlMemoryPressure::HandleEmergency() {
  LOG(WARNING) << "OwlMemoryPressure: EMERGENCY tier - "
               << "system memory at " << static_cast<int>(current_usage_ * 100)
               << "%. Requesting aggressive tab discard.";

  // Emergency: signal critical pressure and let OwlTabDiscardPolicy
  // handle the aggressive tab discarding.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace owl
