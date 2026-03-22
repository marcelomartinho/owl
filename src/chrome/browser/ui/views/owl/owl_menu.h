// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OWL_OWL_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_OWL_OWL_MENU_H_

// OwlMenu - Simplified browser menu.
//
// Structure:
//   New Tab           Ctrl+T
//   New Window        Ctrl+N
//   New Private Window Ctrl+Shift+N
//   ─────────────────
//   Memory Usage      → owl://memory
//   Tab Manager       → owl://tabs
//   ─────────────────
//   Extensions        → chrome://extensions
//   Ad Blocker        [ON/OFF toggle]
//   ─────────────────
//   Find              Ctrl+F
//   Print             Ctrl+P
//   ─────────────────
//   Settings          → owl://settings
//   Clear Data        → Clear browsing data dialog
//   ─────────────────
//   About Owl
//
// Removed from Chrome menu:
// - Sync / Sign-in
// - Reading list
// - Cast
// - Side panel
// - Most bookmarks UI (simplified)

namespace owl::ui {

class OwlMenu {
 public:
  OwlMenu();
  ~OwlMenu();

  OwlMenu(const OwlMenu&) = delete;
  OwlMenu& operator=(const OwlMenu&) = delete;

  void Initialize();
};

}  // namespace owl::ui

#endif  // CHROME_BROWSER_UI_VIEWS_OWL_OWL_MENU_H_
