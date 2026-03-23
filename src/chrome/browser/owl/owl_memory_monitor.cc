// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/owl_memory_monitor.h"

#include <algorithm>
#include <sstream>

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace owl {

OwlMemoryMonitor::OwlMemoryMonitor(const Config& config)
    : config_(config) {}

OwlMemoryMonitor::~OwlMemoryMonitor() {
  Stop();
}

void OwlMemoryMonitor::Start() {
  VLOG(1) << "OwlMemoryMonitor: Starting (interval: "
          << config_.report_interval.InSeconds() << "s)";
  report_timer_.Start(FROM_HERE, config_.report_interval,
                      base::BindRepeating(&OwlMemoryMonitor::OnReportTimer,
                                          base::Unretained(this)));
  // Collect initial report immediately.
  CollectReports();
}

void OwlMemoryMonitor::Stop() {
  report_timer_.Stop();
}

MemoryReport OwlMemoryMonitor::GetFullReport() const {
  return latest_report_;
}

std::map<int, uint64_t> OwlMemoryMonitor::GetPerTabMemory() const {
  return per_tab_memory_;
}

std::vector<ExtensionMemoryReport> OwlMemoryMonitor::GetExtensionMemory()
    const {
  return extension_memory_;
}

uint64_t OwlMemoryMonitor::GetTotalMemoryBytes() const {
  return total_memory_bytes_;
}

uint64_t OwlMemoryMonitor::GetUnclassifiedMemoryBytes() const {
  // Calculate the sum of all reported subsystems.
  uint64_t classified = 0;
  for (const auto& child : latest_report_.children) {
    classified += child.bytes;
  }

  // Unclassified = total RSS - classified.
  if (total_memory_bytes_ > classified) {
    return total_memory_bytes_ - classified;
  }
  return 0;
}

base::Value::Dict OwlMemoryMonitor::ExportAsJson() const {
  base::Value::Dict root;
  root.Set("timestamp", base::Time::Now().InSecondsFSinceUnixEpoch());
  root.Set("total_bytes", static_cast<double>(total_memory_bytes_));
  root.Set("total_human", FormatBytes(total_memory_bytes_));
  root.Set("unclassified_bytes",
           static_cast<double>(GetUnclassifiedMemoryBytes()));

  // Per-tab memory.
  base::Value::Dict tabs;
  for (const auto& [tab_id, bytes] : per_tab_memory_) {
    base::Value::Dict tab;
    tab.Set("bytes", static_cast<double>(bytes));
    tab.Set("human", FormatBytes(bytes));
    tabs.Set(std::to_string(tab_id), std::move(tab));
  }
  root.Set("tabs", std::move(tabs));

  // Extension memory.
  base::Value::List extensions;
  for (const auto& ext : extension_memory_) {
    base::Value::Dict ext_dict;
    ext_dict.Set("id", ext.extension_id);
    ext_dict.Set("name", ext.extension_name);
    ext_dict.Set("bytes", static_cast<double>(ext.memory_bytes));
    ext_dict.Set("human", FormatBytes(ext.memory_bytes));
    ext_dict.Set("is_hog", ext.is_memory_hog);
    extensions.Append(std::move(ext_dict));
  }
  root.Set("extensions", std::move(extensions));

  return root;
}

std::string OwlMemoryMonitor::GetSummaryString() const {
  return FormatBytes(total_memory_bytes_);
}

void OwlMemoryMonitor::OnReportTimer() {
  CollectReports();
}

void OwlMemoryMonitor::CollectReports() {
  TRACE_EVENT0("browser", "OwlMemoryMonitor::CollectReports");

  latest_report_ = MemoryReport{};
  latest_report_.name = "Owl Browser";
  latest_report_.path = "";

  CollectProcessMetrics();
  CollectTabMetrics();
  CollectExtensionMetrics();

  // Sum children to get total classified.
  latest_report_.bytes = total_memory_bytes_;
  latest_report_.description = "Total browser memory: " +
                               FormatBytes(total_memory_bytes_);

  // Log unclassified for developers to identify missing reporters.
  const auto unclassified = GetUnclassifiedMemoryBytes();
  if (unclassified > 0) {
    VLOG(2) << "OwlMemoryMonitor: heap-unclassified = "
            << FormatBytes(unclassified)
            << " (" << (unclassified * 100 / std::max(total_memory_bytes_,
                                                       uint64_t{1}))
            << "% of total)";
  }
}

void OwlMemoryMonitor::CollectProcessMetrics() {
  // In the actual Chromium integration, this uses:
  //   content::BrowserChildProcessHost::GetAll() to get child processes
  //   base::ProcessMetrics::CreateProcessMetrics(pid) for per-process data
  //
  // For now, collect the browser process's own metrics.
  auto metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  total_memory_bytes_ = metrics->GetResidentSetSize();
#else
  size_t private_bytes = 0;
  size_t shared_bytes = 0;
  metrics->GetMemoryBytes(&private_bytes, &shared_bytes);
  total_memory_bytes_ = private_bytes + shared_bytes;
#endif

  // Add browser process report.
  MemoryReport browser;
  browser.name = "Browser Process";
  browser.path = "browser";
  browser.bytes = total_memory_bytes_;
  browser.description = "Main browser process";
  latest_report_.children.push_back(std::move(browser));
}

void OwlMemoryMonitor::CollectTabMetrics() {
  // In the actual Chromium integration, this would:
  // 1. Iterate all WebContents via TabStripModel
  // 2. For each tab, find its renderer process via RenderProcessHost
  // 3. Query app.getAppMetrics() equivalent for per-process memory
  // 4. Map PID -> tab via WebContents::GetMainFrame()->GetProcess()
  //
  // This is a placeholder for the integration point.
  per_tab_memory_.clear();
}

void OwlMemoryMonitor::CollectExtensionMetrics() {
  // In the actual Chromium integration, this would:
  // 1. Iterate ExtensionRegistry::enabled_extensions()
  // 2. For each extension, find its background page/service worker process
  // 3. Query process metrics for memory
  // 4. Flag extensions exceeding the hog threshold
  //
  // This is a placeholder for the integration point.
  extension_memory_.clear();
}

// static
std::string OwlMemoryMonitor::FormatBytes(uint64_t bytes) {
  if (bytes >= 1024ULL * 1024 * 1024) {
    double gb = static_cast<double>(bytes) / (1024.0 * 1024 * 1024);
    std::ostringstream oss;
    oss.precision(1);
    oss << std::fixed << gb << " GB";
    return oss.str();
  }
  if (bytes >= 1024ULL * 1024) {
    uint64_t mb = bytes / (1024 * 1024);
    return std::to_string(mb) + " MB";
  }
  if (bytes >= 1024) {
    uint64_t kb = bytes / 1024;
    return std::to_string(kb) + " KB";
  }
  return std::to_string(bytes) + " B";
}

}  // namespace owl
