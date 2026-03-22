// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_PAGES_OWL_TABS_PAGE_H_
#define CHROME_BROWSER_OWL_PAGES_OWL_TABS_PAGE_H_

#include <string>

namespace owl::pages {

// OwlTabsPage generates the HTML content for the owl://tabs page.
//
// Inspired by Firefox's about:unloads. Shows:
// 1. All open tabs with their LRU priority
// 2. Estimated memory per tab
// 3. Tab status (active, frozen, suspended, discarded)
// 4. Protection status (pinned, audio, WebRTC, unsaved forms)
// 5. Manual unload/discard buttons per tab
// 6. Memory pressure tier indicator
class OwlTabsPage {
 public:
  static std::string GenerateHtml();
  static constexpr const char* kUrl = "owl://tabs";
  static constexpr const char* kHost = "tabs";
};

}  // namespace owl::pages

#endif  // CHROME_BROWSER_OWL_PAGES_OWL_TABS_PAGE_H_
