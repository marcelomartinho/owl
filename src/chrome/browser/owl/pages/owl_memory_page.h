// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_PAGES_OWL_MEMORY_PAGE_H_
#define CHROME_BROWSER_OWL_PAGES_OWL_MEMORY_PAGE_H_

#include <string>

namespace owl::pages {

// OwlMemoryPage generates the HTML content for the owl://memory page.
//
// This page provides a hierarchical breakdown of browser memory usage,
// inspired by Firefox's about:memory. It shows:
//
// 1. Total browser RSS
// 2. Per-subsystem breakdown (DOM, layout, CSS, JS, images, etc.)
// 3. Per-tab memory (with tab names)
// 4. Per-extension memory (with "hog" warnings)
// 5. Heap-unclassified (memory not tracked by any reporter)
// 6. GC statistics (cycles, pause times, bytes reclaimed)
// 7. Memory pressure tier history
//
// The page auto-refreshes every 5 seconds and supports:
// - JSON export for CI regression detection
// - Manual GC trigger button
// - Comparison with previous snapshots
class OwlMemoryPage {
 public:
  // Generate the full HTML page content.
  static std::string GenerateHtml();

  // Generate JSON data for the API endpoint.
  static std::string GenerateJson();

  // The URL for this page.
  static constexpr const char* kUrl = "owl://memory";
  static constexpr const char* kHost = "memory";
};

}  // namespace owl::pages

#endif  // CHROME_BROWSER_OWL_PAGES_OWL_MEMORY_PAGE_H_
