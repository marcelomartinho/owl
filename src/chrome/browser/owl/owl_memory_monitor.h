// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_OWL_MEMORY_MONITOR_H_
#define CHROME_BROWSER_OWL_OWL_MEMORY_MONITOR_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/timer/timer.h"
#include "base/values.h"

namespace owl {

// Memory usage report for a single subsystem.
struct MemoryReport {
  std::string name;              // e.g., "dom", "layout", "css", "js"
  std::string path;              // e.g., "renderer/tab-1/dom"
  uint64_t bytes = 0;            // Current usage in bytes
  uint64_t peak_bytes = 0;       // Peak usage
  std::string description;       // Human-readable description
  std::vector<MemoryReport> children;  // Sub-reports for hierarchy
};

// Per-extension memory report.
struct ExtensionMemoryReport {
  std::string extension_id;
  std::string extension_name;
  uint64_t memory_bytes = 0;
  uint64_t peak_memory_bytes = 0;
  bool is_memory_hog = false;    // True if exceeds threshold
};

// OwlMemoryMonitor - Hierarchical memory reporter framework.
//
// Inspired by Firefox about:memory. Each subsystem reports its memory
// usage hierarchically. The data feeds:
//   - owl://memory page (interactive breakdown)
//   - owl://tabs page (per-tab memory)
//   - OwlMemoryIndicator in the toolbar (badge)
//   - JSON export for CI regression detection
//
// Reports are updated every N seconds (configurable).
class OwlMemoryMonitor {
 public:
  // Threshold for "memory hog" extension (default: 100MB).
  static constexpr uint64_t kExtensionMemoryHogThreshold = 100 * 1024 * 1024;

  struct Config {
    base::TimeDelta report_interval = base::Seconds(5);
    uint64_t extension_hog_threshold = kExtensionMemoryHogThreshold;
  };

  explicit OwlMemoryMonitor(const Config& config = Config{});
  ~OwlMemoryMonitor();

  OwlMemoryMonitor(const OwlMemoryMonitor&) = delete;
  OwlMemoryMonitor& operator=(const OwlMemoryMonitor&) = delete;

  void Start();
  void Stop();

  // Get the current full memory report hierarchy.
  MemoryReport GetFullReport() const;

  // Get per-tab memory breakdown.
  std::map<int, uint64_t> GetPerTabMemory() const;

  // Get per-extension memory breakdown.
  std::vector<ExtensionMemoryReport> GetExtensionMemory() const;

  // Get total browser memory (RSS).
  uint64_t GetTotalMemoryBytes() const;

  // Get "heap-unclassified" - memory not reported by any subsystem.
  // A high value here indicates missing reporters.
  uint64_t GetUnclassifiedMemoryBytes() const;

  // Export the full report as JSON for CI/regression detection.
  base::Value::Dict ExportAsJson() const;

  // Export compact summary string for the toolbar indicator.
  // Format: "1.2 GB" or "856 MB"
  std::string GetSummaryString() const;

 private:
  // Collect reports from all subsystems.
  void CollectReports();

  // Collect memory from Chrome's process metrics.
  void CollectProcessMetrics();

  // Collect per-tab memory via renderer process mapping.
  void CollectTabMetrics();

  // Collect extension memory.
  void CollectExtensionMetrics();

  // Timer callback.
  void OnReportTimer();

  // Format bytes as human-readable string.
  static std::string FormatBytes(uint64_t bytes);

  Config config_;
  base::RepeatingTimer report_timer_;
  MemoryReport latest_report_;
  std::map<int, uint64_t> per_tab_memory_;
  std::vector<ExtensionMemoryReport> extension_memory_;
  uint64_t total_memory_bytes_ = 0;
};

}  // namespace owl

#endif  // CHROME_BROWSER_OWL_OWL_MEMORY_MONITOR_H_
