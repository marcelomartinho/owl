// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OWL_OWL_MEMORY_INDICATOR_H_
#define CHROME_BROWSER_UI_VIEWS_OWL_OWL_MEMORY_INDICATOR_H_

#include <cstdint>
#include <string>

// OwlMemoryIndicator - Toolbar badge showing browser memory usage.
//
// Shows total browser RSS in a compact badge: "1.2 GB"
// Color-coded by memory pressure tier:
//   Green: None
//   Yellow: Warning
//   Red: Critical/Emergency
//
// Click opens owl://memory for detailed breakdown.
// Hover shows tooltip with:
//   - Total RSS
//   - Tab count (active / frozen / discarded)
//   - Memory pressure tier
//   - Memory hog extensions (if any)

namespace owl::ui {

class OwlMemoryIndicator {
 public:
  OwlMemoryIndicator();
  ~OwlMemoryIndicator();

  OwlMemoryIndicator(const OwlMemoryIndicator&) = delete;
  OwlMemoryIndicator& operator=(const OwlMemoryIndicator&) = delete;

  // Update the displayed memory value and color.
  void Update(uint64_t total_bytes, int pressure_tier);

  // Get the display text (e.g., "1.2 GB").
  std::string GetDisplayText() const;

 private:
  uint64_t total_bytes_ = 0;
  int pressure_tier_ = 0;
};

}  // namespace owl::ui

#endif  // CHROME_BROWSER_UI_VIEWS_OWL_OWL_MEMORY_INDICATOR_H_
