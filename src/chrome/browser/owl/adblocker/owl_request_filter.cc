// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/adblocker/owl_request_filter.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace owl::adblocker {

OwlRequestFilter::OwlRequestFilter() = default;
OwlRequestFilter::~OwlRequestFilter() = default;

bool OwlRequestFilter::Initialize(const uint8_t* flatbuffer_data,
                                  size_t flatbuffer_size) {
  if (!flatbuffer_data || flatbuffer_size == 0) {
    LOG(ERROR) << "OwlRequestFilter: Invalid filter data";
    return false;
  }

  filter_data_ = flatbuffer_data;
  filter_data_size_ = flatbuffer_size;
  is_initialized_ = true;

  stats_.memory_bytes = flatbuffer_size;

  // In the actual implementation, this would:
  // 1. Verify the FlatBuffer schema
  // 2. Build the trie/hash index from the FlatBuffer data
  // 3. Count the loaded filters
  //
  // The FlatBuffer is memory-mapped, so there's zero deserialization
  // overhead. The data is accessed directly from the mmap'd region.

  VLOG(1) << "OwlRequestFilter: Initialized with "
          << flatbuffer_size << " bytes of filter data";

  return true;
}

FilterResult OwlRequestFilter::ShouldBlock(
    const GURL& url,
    const GURL& first_party_url,
    ResourceType resource_type) const {
  TRACE_EVENT0("browser", "OwlRequestFilter::ShouldBlock");

  FilterResult result;

  if (!is_initialized_) {
    return result;
  }

  stats_.requests_checked++;

  // Check whitelist first.
  if (IsWhitelisted(url, first_party_url)) {
    stats_.requests_whitelisted++;
    return result;
  }

  // Check block filters.
  const std::string url_string = url.spec();
  const std::string domain(url.host());

  if (MatchesFilter(url_string, domain, resource_type)) {
    result.should_block = true;
    result.matched_list = "easylist";  // Simplified for now.

    stats_.requests_blocked++;

    // Estimate bytes saved (average web request size).
    // This is a rough heuristic; actual savings vary by resource type.
    uint64_t estimated_size = 0;
    switch (resource_type) {
      case ResourceType::kScript:
        estimated_size = 50 * 1024;  // ~50KB average
        break;
      case ResourceType::kImage:
        estimated_size = 100 * 1024;  // ~100KB average
        break;
      case ResourceType::kStylesheet:
        estimated_size = 20 * 1024;  // ~20KB average
        break;
      case ResourceType::kMedia:
        estimated_size = 500 * 1024;  // ~500KB average
        break;
      default:
        estimated_size = 10 * 1024;  // ~10KB default
        break;
    }
    stats_.bytes_saved_estimate += estimated_size;

    VLOG(2) << "OwlRequestFilter: Blocked " << url_string;
  }

  return result;
}

bool OwlRequestFilter::IsWhitelisted(const GURL& url,
                                     const GURL& first_party_url) const {
  // In the actual implementation, this would check exception rules
  // (lines starting with @@ in EasyList format) in the FlatBuffer.
  //
  // Exception rules allow specific URLs to bypass blocking,
  // typically used for first-party resources that match generic
  // blocking patterns.
  return false;
}

bool OwlRequestFilter::MatchesFilter(const std::string& url_string,
                                     const std::string& domain,
                                     ResourceType type) const {
  // In the actual implementation, this would:
  // 1. Look up the URL in the FlatBuffer trie/hash index
  // 2. Check domain-specific rules
  // 3. Apply resource type constraints
  // 4. Handle wildcard and regex patterns
  //
  // The FlatBuffer layout would be:
  //   - Table of domain hash -> filter indices
  //   - Table of URL pattern hash -> filter indices
  //   - Array of filter rules with: pattern, domains, resource types, options
  //
  // Brave's implementation showed that FlatBuffers reduce memory by 75%
  // compared to parsing EasyList text format into in-memory data structures.
  //
  // Common ad/tracker domain patterns that would be in the filter list:
  //   - doubleclick.net
  //   - googlesyndication.com
  //   - facebook.com/tr
  //   - analytics.google.com
  //   - Tracking pixels (1x1 images)
  //   - Third-party script loaders

  // Placeholder: no actual filtering until FlatBuffer integration.
  return false;
}

}  // namespace owl::adblocker
