// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/owl/owl_memory_indicator.h"

#include <sstream>

namespace owl::ui {

OwlMemoryIndicator::OwlMemoryIndicator() = default;
OwlMemoryIndicator::~OwlMemoryIndicator() = default;

void OwlMemoryIndicator::Update(uint64_t total_bytes, int pressure_tier) {
  total_bytes_ = total_bytes;
  pressure_tier_ = pressure_tier;
  // In actual integration: invalidate the view to trigger repaint
  // with updated text and color.
}

std::string OwlMemoryIndicator::GetDisplayText() const {
  if (total_bytes_ >= 1024ULL * 1024 * 1024) {
    double gb = static_cast<double>(total_bytes_) / (1024.0 * 1024 * 1024);
    std::ostringstream oss;
    oss.precision(1);
    oss << std::fixed << gb << " GB";
    return oss.str();
  }
  uint64_t mb = total_bytes_ / (1024 * 1024);
  return std::to_string(mb) + " MB";
}

}  // namespace owl::ui
