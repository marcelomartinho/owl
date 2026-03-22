// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/adblocker/owl_filter_list_manager.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/owl/adblocker/owl_request_filter.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace owl::adblocker {

const char OwlFilterListManager::kEasyListUrl[] =
    "https://easylist.to/easylist/easylist.txt";
const char OwlFilterListManager::kEasyPrivacyUrl[] =
    "https://easylist.to/easylist/easyprivacy.txt";

OwlFilterListManager::OwlFilterListManager(const base::FilePath& data_dir)
    : data_dir_(data_dir) {
  // Register default filter lists.
  filter_lists_.push_back({
      .name = "EasyList",
      .url = kEasyListUrl,
      .text_path = data_dir_.AppendASCII("easylist.txt"),
      .binary_path = data_dir_.AppendASCII("easylist.fb"),
  });
  filter_lists_.push_back({
      .name = "EasyPrivacy",
      .url = kEasyPrivacyUrl,
      .text_path = data_dir_.AppendASCII("easyprivacy.txt"),
      .binary_path = data_dir_.AppendASCII("easyprivacy.fb"),
  });
}

OwlFilterListManager::~OwlFilterListManager() = default;

void OwlFilterListManager::Initialize(OwlRequestFilter* filter,
                                      ReadyCallback callback) {
  TRACE_EVENT0("browser", "OwlFilterListManager::Initialize");
  filter_ = filter;

  // Try to load cached FlatBuffers first (fast path).
  bool loaded = false;
  for (const auto& list : filter_lists_) {
    if (base::PathExists(list.binary_path)) {
      if (LoadFlatBuffer(list.binary_path, filter)) {
        loaded = true;
        VLOG(1) << "OwlFilterList: Loaded cached " << list.name;
      }
    }
  }

  if (loaded) {
    // Check if we need background updates.
    for (const auto& list : filter_lists_) {
      if (NeedsUpdate(list)) {
        VLOG(1) << "OwlFilterList: " << list.name
                << " needs update (>24h old)";
        // Schedule background update (don't block startup).
        // In actual implementation: PostTask to download + recompile.
      }
    }
    std::move(callback).Run(true);
    return;
  }

  // No cached data: need to download and compile.
  // In actual implementation, this would:
  // 1. Download each list URL asynchronously
  // 2. Parse the text format (EasyList AdBlock format)
  // 3. Compile to FlatBuffer binary
  // 4. Write to disk
  // 5. Load via mmap
  VLOG(1) << "OwlFilterList: No cached lists, download required";
  std::move(callback).Run(false);
}

void OwlFilterListManager::ForceUpdate(ReadyCallback callback) {
  TRACE_EVENT0("browser", "OwlFilterListManager::ForceUpdate");
  // Download all lists and recompile.
  // Placeholder: actual implementation needs async network requests.
  VLOG(1) << "OwlFilterList: Force update requested";
  std::move(callback).Run(false);
}

std::vector<FilterListInfo> OwlFilterListManager::GetFilterListInfo() const {
  return filter_lists_;
}

void OwlFilterListManager::AddFilterList(const std::string& name,
                                         const std::string& url) {
  FilterListInfo info;
  info.name = name;
  info.url = url;
  info.text_path = data_dir_.AppendASCII(name + ".txt");
  info.binary_path = data_dir_.AppendASCII(name + ".fb");
  filter_lists_.push_back(std::move(info));
  VLOG(1) << "OwlFilterList: Added custom list: " << name;
}

void OwlFilterListManager::RemoveFilterList(const std::string& name) {
  filter_lists_.erase(
      std::remove_if(filter_lists_.begin(), filter_lists_.end(),
                     [&](const auto& l) { return l.name == name; }),
      filter_lists_.end());
}

bool OwlFilterListManager::CompileToFlatBuffer(
    const base::FilePath& text_path,
    const base::FilePath& binary_path) {
  TRACE_EVENT0("browser", "OwlFilterListManager::CompileToFlatBuffer");

  // In the actual implementation, this would:
  //
  // 1. Read the EasyList text file line by line
  // 2. Parse each line into a filter rule:
  //    - Block rules: ||domain.com^ or /pattern/
  //    - Exception rules: @@||domain.com^
  //    - Options: $script,third-party,domain=...
  //    - Comments: ! or [Adblock Plus ...]
  //
  // 3. Build FlatBuffer schema:
  //    table FilterRule {
  //      pattern: string;
  //      pattern_type: PatternType;  // domain, url, regex
  //      domains: [string];          // applicable domains
  //      resource_types: uint32;     // bitmask of ResourceType
  //      is_exception: bool;
  //      options: uint32;            // third-party, etc.
  //    }
  //    table FilterDatabase {
  //      version: uint32;
  //      rules: [FilterRule];
  //      domain_index: [DomainEntry]; // hash -> rule indices
  //    }
  //
  // 4. Serialize to FlatBuffer binary and write to disk
  //
  // The FlatBuffer binary can be memory-mapped (mmap) at startup,
  // giving zero deserialization overhead. Brave demonstrated 75%
  // memory reduction with this approach.

  LOG(INFO) << "OwlFilterList: Compiling " << text_path.value()
            << " -> " << binary_path.value();
  return false;  // Placeholder.
}

bool OwlFilterListManager::LoadFlatBuffer(const base::FilePath& binary_path,
                                          OwlRequestFilter* filter) {
  TRACE_EVENT0("browser", "OwlFilterListManager::LoadFlatBuffer");

#if defined(OS_POSIX)
  // Memory-map the FlatBuffer file for zero-copy access.
  // This is the key performance optimization: the OS handles
  // paging the data in/out as needed, and multiple processes
  // can share the same physical pages.

  int fd = open(binary_path.value().c_str(), O_RDONLY);
  if (fd < 0) {
    VLOG(1) << "OwlFilterList: Cannot open " << binary_path.value();
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return false;
  }

  void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  if (data == MAP_FAILED) {
    LOG(ERROR) << "OwlFilterList: mmap failed for " << binary_path.value();
    return false;
  }

  // Tell the OS we'll read this sequentially (improves readahead).
  madvise(data, st.st_size, MADV_SEQUENTIAL);

  return filter->Initialize(static_cast<const uint8_t*>(data), st.st_size);
#else
  // Non-POSIX: read the file into memory.
  std::string content;
  if (!base::ReadFileToString(binary_path, &content)) {
    return false;
  }
  // Note: this copies the data, losing the mmap benefit.
  // Windows could use CreateFileMapping for equivalent behavior.
  return filter->Initialize(
      reinterpret_cast<const uint8_t*>(content.data()), content.size());
#endif
}

bool OwlFilterListManager::NeedsUpdate(const FilterListInfo& list) const {
  if (list.last_updated.is_null()) {
    return true;  // Never downloaded.
  }
  const auto age = base::Time::Now() - list.last_updated;
  return age > base::Hours(24);
}

}  // namespace owl::adblocker
