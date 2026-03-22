// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OWL_OWL_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_OWL_OWL_TAB_STRIP_H_

#include <cstdint>

// OwlTabStrip - Minimal tab strip with per-tab memory indicators.
//
// Visual design:
// - Compact tabs (less padding than Chrome default)
// - Small memory indicator per tab (color-coded):
//   Green: < 100MB
//   Yellow: 100-300MB
//   Red: > 300MB
// - Frozen/suspended tab indicator (snowflake icon or dimmed)
// - Discarded tab indicator (crossed-out)
// - Pinned tabs are smaller and show only favicon

namespace owl::ui {

class OwlTabStrip {
 public:
  OwlTabStrip();
  ~OwlTabStrip();

  OwlTabStrip(const OwlTabStrip&) = delete;
  OwlTabStrip& operator=(const OwlTabStrip&) = delete;

  // Update the memory indicator for a specific tab.
  void UpdateTabMemory(int tab_id, uint64_t memory_bytes);

  // Update the tab status (active, frozen, suspended, discarded).
  void UpdateTabStatus(int tab_id, bool is_frozen, bool is_discarded);

  // Memory thresholds for color coding.
  static constexpr uint64_t kGreenThreshold = 100 * 1024 * 1024;   // 100MB
  static constexpr uint64_t kYellowThreshold = 300 * 1024 * 1024;  // 300MB
};

}  // namespace owl::ui

#endif  // CHROME_BROWSER_UI_VIEWS_OWL_OWL_TAB_STRIP_H_
