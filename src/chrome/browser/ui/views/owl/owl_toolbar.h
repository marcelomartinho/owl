// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OWL_OWL_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_OWL_OWL_TOOLBAR_H_

// OwlToolbar - Simplified toolbar layout.
//
// Layout: [Back] [Fwd] [Reload] [Address Bar] [Memory Badge] [Extensions] [Menu]
//
// Compared to Chrome:
// - No avatar button
// - No bookmarks star (in toolbar; available in menu)
// - Memory badge replaces Chrome's "Performance" indicator
// - Simplified extension icons area

namespace owl::ui {

class OwlToolbar {
 public:
  OwlToolbar();
  ~OwlToolbar();

  OwlToolbar(const OwlToolbar&) = delete;
  OwlToolbar& operator=(const OwlToolbar&) = delete;

  void Initialize();
};

}  // namespace owl::ui

#endif  // CHROME_BROWSER_UI_VIEWS_OWL_OWL_TOOLBAR_H_
