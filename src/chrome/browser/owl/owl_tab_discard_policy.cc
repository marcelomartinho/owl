// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/owl_tab_discard_policy.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace owl {

OwlTabDiscardPolicy::OwlTabDiscardPolicy(const Config& config)
    : config_(config) {}

OwlTabDiscardPolicy::~OwlTabDiscardPolicy() = default;

void OwlTabDiscardPolicy::OnOwlMemoryPressureChanged(
    MemoryPressureTier tier) {
  TRACE_EVENT1("browser", "OwlTabDiscardPolicy::OnOwlMemoryPressureChanged",
               "tier", static_cast<int>(tier));

  // Tab discard actions are triggered by the memory pressure system
  // and executed by the browser's tab lifecycle manager.
  // This callback notifies the policy of tier changes; the actual
  // discard decisions are made when GetTabsToDiscard() is called.

  switch (tier) {
    case MemoryPressureTier::kWarning:
      VLOG(1) << "OwlTabDiscard: Warning tier - will freeze background tabs";
      break;
    case MemoryPressureTier::kCritical:
      VLOG(1) << "OwlTabDiscard: Critical tier - will discard LRU tabs";
      break;
    case MemoryPressureTier::kEmergency:
      LOG(WARNING) << "OwlTabDiscard: Emergency tier - aggressive discard";
      break;
    default:
      break;
  }
}

std::vector<TabDiscardPriority> OwlTabDiscardPolicy::CalculatePriorities(
    const std::vector<TabInfo>& tabs) const {
  TRACE_EVENT1("browser", "OwlTabDiscardPolicy::CalculatePriorities",
               "tab_count", tabs.size());

  const auto now = base::TimeTicks::Now();

  // Find max inactive time and max memory for normalization.
  base::TimeDelta max_inactive_time;
  uint64_t max_memory = 1;  // Avoid division by zero.

  for (const auto& tab : tabs) {
    if (!tab.is_active) {
      const auto inactive_time = now - tab.last_active_time;
      if (inactive_time > max_inactive_time) {
        max_inactive_time = inactive_time;
      }
    }
    if (tab.estimated_memory_bytes > max_memory) {
      max_memory = tab.estimated_memory_bytes;
    }
  }

  // Calculate priority for each tab.
  std::vector<TabDiscardPriority> priorities;
  priorities.reserve(tabs.size());

  for (const auto& tab : tabs) {
    TabDiscardPriority priority;
    priority.tab_id = tab.tab_id;
    priority.priority_score =
        CalculateTabScore(tab, now, max_inactive_time, max_memory);

    // Generate human-readable reason.
    if (tab.is_active) {
      priority.reason = "Active tab";
    } else if (tab.is_playing_audio) {
      priority.reason = "Playing audio";
    } else if (tab.has_webrtc_session) {
      priority.reason = "WebRTC session";
    } else if (tab.is_pinned) {
      priority.reason = "Pinned tab";
    } else if (tab.has_unsaved_form_data) {
      priority.reason = "Unsaved form data";
    } else {
      const auto idle_minutes =
          (now - tab.last_active_time).InMinutes();
      const auto mem_mb = tab.estimated_memory_bytes / (1024 * 1024);
      priority.reason = "Idle " + std::to_string(idle_minutes) +
                        "min, " + std::to_string(mem_mb) + "MB";
    }

    priorities.push_back(std::move(priority));
  }

  // Sort by priority (lowest score = discard first).
  std::sort(priorities.begin(), priorities.end(),
            [](const auto& a, const auto& b) {
              return a.priority_score < b.priority_score;
            });

  return priorities;
}

std::vector<int> OwlTabDiscardPolicy::GetTabsToDiscard(
    const std::vector<TabInfo>& tabs,
    MemoryPressureTier tier) const {
  TRACE_EVENT2("browser", "OwlTabDiscardPolicy::GetTabsToDiscard",
               "tab_count", tabs.size(),
               "tier", static_cast<int>(tier));

  if (tier == MemoryPressureTier::kNone ||
      tier == MemoryPressureTier::kWarning) {
    // No discarding at None or Warning tier.
    return {};
  }

  auto priorities = CalculatePriorities(tabs);
  std::vector<int> to_discard;

  int max_discard;
  if (tier == MemoryPressureTier::kEmergency) {
    // Emergency: discard everything except the top N tabs.
    max_discard = static_cast<int>(tabs.size()) -
                  config_.emergency_keep_alive_count;
  } else {
    // Critical: discard up to max_discard_per_event.
    max_discard = config_.max_discard_per_event;
  }

  const auto now = base::TimeTicks::Now();

  for (const auto& priority : priorities) {
    if (static_cast<int>(to_discard.size()) >= max_discard) {
      break;
    }

    // Find the original tab info.
    const auto it = std::find_if(
        tabs.begin(), tabs.end(),
        [&](const auto& t) { return t.tab_id == priority.tab_id; });

    if (it == tabs.end()) continue;

    // Skip protected tabs.
    if (IsProtected(*it)) continue;

    // Skip already discarded tabs.
    if (it->is_discarded) continue;

    // Skip tabs within grace period.
    if (now - it->last_active_time < config_.grace_period) continue;

    to_discard.push_back(priority.tab_id);
  }

  VLOG(1) << "OwlTabDiscard: " << to_discard.size()
          << " tabs selected for discard (tier="
          << static_cast<int>(tier) << ")";

  return to_discard;
}

std::vector<int> OwlTabDiscardPolicy::GetTabsToFreeze(
    const std::vector<TabInfo>& tabs) const {
  std::vector<int> to_freeze;
  const auto now = base::TimeTicks::Now();

  for (const auto& tab : tabs) {
    if (tab.is_active || tab.is_suspended || tab.is_discarded) {
      continue;
    }
    // Freeze if past grace period and not protected.
    if (now - tab.last_active_time >= config_.grace_period &&
        !IsProtected(tab)) {
      to_freeze.push_back(tab.tab_id);
    }
  }

  return to_freeze;
}

bool OwlTabDiscardPolicy::IsProtected(const TabInfo& tab) const {
  // Active tab is always protected.
  if (tab.is_active) return true;

  // Tabs playing audio are protected (user expects them).
  if (tab.is_playing_audio) return true;

  // Tabs with WebRTC sessions are protected (active calls).
  if (tab.has_webrtc_session) return true;

  // Tabs with unsaved form data are protected.
  if (tab.has_unsaved_form_data) return true;

  return false;
}

double OwlTabDiscardPolicy::CalculateTabScore(
    const TabInfo& tab,
    base::TimeTicks now,
    base::TimeDelta max_inactive_time,
    uint64_t max_memory) const {
  // Active tab gets maximum protection.
  if (tab.is_active) return 10.0;

  double score = 0.0;

  // LRU component: recently used tabs get higher score.
  // Normalized to [0, 1] where 1 = most recently used.
  if (max_inactive_time.is_positive()) {
    const auto inactive_time = now - tab.last_active_time;
    const double lru_score =
        1.0 - (static_cast<double>(inactive_time.InSeconds()) /
               max_inactive_time.InSeconds());
    score += lru_score * config_.lru_weight;
  }

  // Memory component: light tabs get higher score (we want to discard
  // heavy tabs first). Only active when > heavy_tab_threshold tabs.
  if (max_memory > 0) {
    const double memory_score =
        1.0 - (static_cast<double>(tab.estimated_memory_bytes) / max_memory);
    score += memory_score * config_.memory_weight;
  }

  // Bonus for pinned tabs.
  if (tab.is_pinned) {
    score += config_.pinned_bonus;
  }

  // Bonus for audio-playing tabs (maximum protection).
  if (tab.is_playing_audio) {
    score += config_.audio_bonus;
  }

  // Bonus for WebRTC tabs.
  if (tab.has_webrtc_session) {
    score += config_.audio_bonus;
  }

  // Bonus for unsaved form data.
  if (tab.has_unsaved_form_data) {
    score += config_.pinned_bonus;
  }

  return score;
}

}  // namespace owl
