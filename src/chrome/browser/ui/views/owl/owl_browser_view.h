// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OWL_OWL_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OWL_OWL_BROWSER_VIEW_H_

// OwlBrowserView - Custom browser view for Owl.
//
// Replaces Chrome's default BrowserView with a minimal layout:
// - Simplified toolbar (back, forward, reload, address bar, memory badge)
// - Tab strip with per-tab memory indicators
// - No bookmarks bar by default (can be toggled)
// - No side panel
// - Reduced padding and margins for more content space
//
// The goal is to minimize UI chrome overhead, both visually and in memory.

namespace owl::ui {

class OwlBrowserView {
 public:
  OwlBrowserView();
  ~OwlBrowserView();

  OwlBrowserView(const OwlBrowserView&) = delete;
  OwlBrowserView& operator=(const OwlBrowserView&) = delete;

  // Initialize the browser view layout.
  void Initialize();

  // Update memory indicators (called periodically).
  void UpdateMemoryDisplay();
};

}  // namespace owl::ui

#endif  // CHROME_BROWSER_UI_VIEWS_OWL_OWL_BROWSER_VIEW_H_
