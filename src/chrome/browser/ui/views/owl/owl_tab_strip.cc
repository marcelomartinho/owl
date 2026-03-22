// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/owl/owl_tab_strip.h"

namespace owl::ui {

OwlTabStrip::OwlTabStrip() = default;
OwlTabStrip::~OwlTabStrip() = default;

void OwlTabStrip::UpdateTabMemory(int tab_id, uint64_t memory_bytes) {
  // In the actual integration, this would:
  // 1. Find the tab view by ID
  // 2. Update the memory indicator color:
  //    - Green dot: < 100MB
  //    - Yellow dot: 100-300MB
  //    - Red dot: > 300MB
  // 3. Update tooltip with exact memory usage
}

void OwlTabStrip::UpdateTabStatus(int tab_id, bool is_frozen,
                                  bool is_discarded) {
  // In the actual integration, this would:
  // 1. Find the tab view by ID
  // 2. If frozen: show snowflake indicator, dim the tab slightly
  // 3. If discarded: show crossed-out indicator, dim further
  // 4. Update tooltip with status info
}

}  // namespace owl::ui
