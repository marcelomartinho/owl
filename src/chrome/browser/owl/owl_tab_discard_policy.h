// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_OWL_TAB_DISCARD_POLICY_H_
#define CHROME_BROWSER_OWL_OWL_TAB_DISCARD_POLICY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/owl/owl_memory_pressure.h"

namespace owl {

// Information about a tab used for discard priority calculation.
struct TabInfo {
  int tab_id = 0;
  std::string title;
  std::string url;
  base::TimeTicks last_active_time;
  uint64_t estimated_memory_bytes = 0;
  int renderer_process_id = 0;
  bool is_pinned = false;
  bool is_playing_audio = false;
  bool has_webrtc_session = false;
  bool has_unsaved_form_data = false;
  bool is_active = false;
  bool is_suspended = false;
  bool is_discarded = false;
};

// Priority for tab discarding. Lower values = discarded first.
struct TabDiscardPriority {
  int tab_id = 0;
  double priority_score = 0.0;  // 0.0 = discard first, 1.0 = protect
  std::string reason;           // Human-readable reason for priority
};

// OwlTabDiscardPolicy implements LRU tab unloading with smart exceptions.
//
// Inspired by Firefox 93 Tab Unloader:
// - Tabs ranked by last interaction time (LRU = discard first)
// - Exceptions: audio, pinned, WebRTC, unsaved forms
// - For >11 tabs: also considers per-tab memory weight
//
// Integrated with OwlMemoryPressure 3-tier system:
// - Warning: freeze background tabs, no discard
// - Critical: discard LRU tabs until under threshold
// - Emergency: aggressive discard, keep only active tab
class OwlTabDiscardPolicy : public OwlMemoryPressureObserver {
 public:
  struct Config {
    // Minimum time in background before eligible for discard.
    base::TimeDelta grace_period = base::Minutes(5);

    // Number of tabs that triggers heavy-tab detection.
    int heavy_tab_threshold = 11;

    // Maximum tabs to discard per pressure event.
    int max_discard_per_event = 3;

    // In emergency mode, keep only N tabs alive.
    int emergency_keep_alive_count = 1;

    // Weight factors for priority calculation.
    double lru_weight = 0.6;        // Last active time importance
    double memory_weight = 0.3;     // Memory consumption importance
    double pinned_bonus = 0.5;      // Priority bonus for pinned tabs
    double audio_bonus = 1.0;       // Priority bonus for audio tabs
  };

  explicit OwlTabDiscardPolicy(const Config& config = Config{});
  ~OwlTabDiscardPolicy() override;

  // OwlMemoryPressureObserver implementation.
  void OnOwlMemoryPressureChanged(MemoryPressureTier tier) override;

  // Calculate discard priorities for all tabs.
  // Returns tabs sorted by priority (discard first = front of list).
  std::vector<TabDiscardPriority> CalculatePriorities(
      const std::vector<TabInfo>& tabs) const;

  // Get tabs that should be discarded given the current pressure tier.
  // Returns tab IDs in order they should be discarded.
  std::vector<int> GetTabsToDiscard(
      const std::vector<TabInfo>& tabs,
      MemoryPressureTier tier) const;

  // Get tabs that should be frozen (but not discarded).
  std::vector<int> GetTabsToFreeze(
      const std::vector<TabInfo>& tabs) const;

  // Check if a specific tab is protected from discarding.
  bool IsProtected(const TabInfo& tab) const;

  const Config& config() const { return config_; }

 private:
  // Calculate priority score for a single tab.
  // Higher score = more protected from discard.
  double CalculateTabScore(const TabInfo& tab,
                           base::TimeTicks now,
                           base::TimeDelta max_inactive_time,
                           uint64_t max_memory) const;

  Config config_;
};

}  // namespace owl

#endif  // CHROME_BROWSER_OWL_OWL_TAB_DISCARD_POLICY_H_
