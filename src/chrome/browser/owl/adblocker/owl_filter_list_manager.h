// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_ADBLOCKER_OWL_FILTER_LIST_MANAGER_H_
#define CHROME_BROWSER_OWL_ADBLOCKER_OWL_FILTER_LIST_MANAGER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace owl::adblocker {

class OwlRequestFilter;

// Information about a filter list.
struct FilterListInfo {
  std::string name;           // e.g., "EasyList", "EasyPrivacy"
  std::string url;            // Download URL
  base::FilePath text_path;   // Path to the raw text file
  base::FilePath binary_path; // Path to the compiled FlatBuffer
  base::Time last_updated;    // When the list was last downloaded
  base::Time last_compiled;   // When the FlatBuffer was last built
  uint64_t filter_count = 0;  // Number of rules in the list
  uint64_t text_size = 0;     // Size of raw text file
  uint64_t binary_size = 0;   // Size of compiled FlatBuffer
};

// OwlFilterListManager - Manages filter list lifecycle.
//
// Responsibilities:
// 1. Download filter lists (EasyList, EasyPrivacy, etc.)
// 2. Compile text lists into FlatBuffer binary format
// 3. Load compiled lists via mmap into OwlRequestFilter
// 4. Update lists periodically (every 24 hours)
//
// The FlatBuffer approach (from Brave) provides:
// - 75% less memory than text-based parsing
// - Zero deserialization overhead (mmap directly)
// - Fast startup (no parsing on launch)
class OwlFilterListManager {
 public:
  // Callback when filter lists are ready for use.
  using ReadyCallback = base::OnceCallback<void(bool success)>;

  explicit OwlFilterListManager(const base::FilePath& data_dir);
  ~OwlFilterListManager();

  OwlFilterListManager(const OwlFilterListManager&) = delete;
  OwlFilterListManager& operator=(const OwlFilterListManager&) = delete;

  // Initialize: load cached FlatBuffers or download+compile fresh lists.
  void Initialize(OwlRequestFilter* filter, ReadyCallback callback);

  // Force update all filter lists from their URLs.
  void ForceUpdate(ReadyCallback callback);

  // Get info about all managed filter lists.
  std::vector<FilterListInfo> GetFilterListInfo() const;

  // Add a custom filter list by URL.
  void AddFilterList(const std::string& name, const std::string& url);

  // Remove a filter list.
  void RemoveFilterList(const std::string& name);

  // Default filter list URLs.
  static const char kEasyListUrl[];
  static const char kEasyPrivacyUrl[];

 private:
  // Download a filter list from its URL.
  void DownloadList(const FilterListInfo& list,
                    base::OnceCallback<void(bool)> callback);

  // Compile a text filter list into FlatBuffer binary format.
  // This is the key optimization: the compiled binary is mmap'd
  // at startup with zero parsing overhead.
  bool CompileToFlatBuffer(const base::FilePath& text_path,
                           const base::FilePath& binary_path);

  // Load a compiled FlatBuffer via mmap.
  bool LoadFlatBuffer(const base::FilePath& binary_path,
                      OwlRequestFilter* filter);

  // Check if a filter list needs updating (> 24 hours old).
  bool NeedsUpdate(const FilterListInfo& list) const;

  base::FilePath data_dir_;
  std::vector<FilterListInfo> filter_lists_;
  OwlRequestFilter* filter_ = nullptr;  // Not owned.
};

}  // namespace owl::adblocker

#endif  // CHROME_BROWSER_OWL_ADBLOCKER_OWL_FILTER_LIST_MANAGER_H_
