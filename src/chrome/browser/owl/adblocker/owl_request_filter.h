// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_ADBLOCKER_OWL_REQUEST_FILTER_H_
#define CHROME_BROWSER_OWL_ADBLOCKER_OWL_REQUEST_FILTER_H_

#include <cstdint>
#include <string>

#include "url/gurl.h"

namespace owl::adblocker {

// Resource types for filter matching.
enum class ResourceType {
  kDocument,
  kSubDocument,
  kStylesheet,
  kScript,
  kImage,
  kFont,
  kMedia,
  kXmlHttpRequest,
  kWebSocket,
  kPing,
  kOther,
};

// Result of a filter match.
struct FilterResult {
  bool should_block = false;
  std::string matched_filter;  // The filter rule that matched.
  std::string matched_list;    // Which filter list (e.g., "easylist").
};

// OwlRequestFilter - Network-level ad/tracker blocker.
//
// Integrates with Chromium's webRequest API to intercept and block
// requests before they reach the renderer. This saves:
//   - Network bandwidth (blocked requests never download)
//   - Memory (no DOM nodes, no rendering, no JS execution)
//   - CPU (no layout/paint for blocked content)
//
// Filter lists are compiled into FlatBuffer binary format for efficiency.
// See OwlFilterListManager for the compilation and update pipeline.
//
// Origin: Brave adblock engine (FlatBuffers approach = 75% less memory).
class OwlRequestFilter {
 public:
  OwlRequestFilter();
  ~OwlRequestFilter();

  OwlRequestFilter(const OwlRequestFilter&) = delete;
  OwlRequestFilter& operator=(const OwlRequestFilter&) = delete;

  // Initialize with compiled filter data (FlatBuffer binary).
  // The data is memory-mapped; this just stores the pointer.
  bool Initialize(const uint8_t* flatbuffer_data, size_t flatbuffer_size);

  // Check if a request should be blocked.
  // Called from webRequest.onBeforeRequest handler.
  FilterResult ShouldBlock(const GURL& url,
                           const GURL& first_party_url,
                           ResourceType resource_type) const;

  // Check if a request matches an exception (whitelist) rule.
  bool IsWhitelisted(const GURL& url,
                     const GURL& first_party_url) const;

  // Statistics for owl://memory and UI.
  struct Stats {
    uint64_t requests_checked = 0;
    uint64_t requests_blocked = 0;
    uint64_t requests_whitelisted = 0;
    uint64_t bytes_saved_estimate = 0;  // Estimated bytes not downloaded.
    uint64_t filter_count = 0;          // Number of loaded filters.
    uint64_t memory_bytes = 0;          // Memory used by filter engine.
  };

  const Stats& stats() const { return stats_; }

  // Returns true if the filter engine is initialized and ready.
  bool is_ready() const { return is_initialized_; }

 private:
  // Internal matching engine using the FlatBuffer data.
  // In the actual implementation, this would use a trie or
  // hash-based lookup optimized for URL pattern matching.
  bool MatchesFilter(const std::string& url_string,
                     const std::string& domain,
                     ResourceType type) const;

  bool is_initialized_ = false;
  const uint8_t* filter_data_ = nullptr;
  size_t filter_data_size_ = 0;
  Stats stats_;
};

}  // namespace owl::adblocker

#endif  // CHROME_BROWSER_OWL_ADBLOCKER_OWL_REQUEST_FILTER_H_
