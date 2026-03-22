// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OWL_OWL_FEATURE_FLAGS_H_
#define CHROME_BROWSER_OWL_OWL_FEATURE_FLAGS_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace owl::features {

// Master switch for all Owl memory optimizations.
BASE_DECLARE_FEATURE(kOwlMemoryOptimization);

// Oilpan GC: aggressive collection for background tabs.
BASE_DECLARE_FEATURE(kOwlAggressiveGC);

// Background GC interval in seconds (default: 5).
extern const base::FeatureParam<int> kOwlBackgroundGCIntervalSeconds;

// Maximum GC pause in milliseconds for foreground tabs (default: 10).
extern const base::FeatureParam<int> kOwlMaxGCPauseMs;

// IsoHeap: type-isolated memory spaces for DOM types.
BASE_DECLARE_FEATURE(kOwlIsoHeap);

// Arena allocator for per-document layout data.
BASE_DECLARE_FEATURE(kOwlArenaAllocator);

// Arena chunk size in bytes (default: 65536).
extern const base::FeatureParam<int> kOwlArenaChunkSize;

// Built-in ad/tracker blocker.
BASE_DECLARE_FEATURE(kOwlAdBlocker);

// Use FlatBuffers for compiled filter lists (default: true).
extern const base::FeatureParam<bool> kOwlAdBlockerUseFlatBuffers;

// Custom minimal UI.
BASE_DECLARE_FEATURE(kOwlCustomUI);

// 3-tier memory pressure response system.
BASE_DECLARE_FEATURE(kOwlMemoryPressureTiers);

// Memory pressure thresholds (fraction of total RAM, 0.0 - 1.0).
extern const base::FeatureParam<double> kOwlPressureWarningThreshold;
extern const base::FeatureParam<double> kOwlPressureCriticalThreshold;
extern const base::FeatureParam<double> kOwlPressureEmergencyThreshold;

// LRU tab unloading.
BASE_DECLARE_FEATURE(kOwlTabLRUUnloading);

// Minimum time (seconds) before a background tab can be unloaded.
extern const base::FeatureParam<int> kOwlTabUnloadGracePeriodSeconds;

// Maximum number of tabs before heavy-tab detection activates.
extern const base::FeatureParam<int> kOwlHeavyTabDetectionThreshold;

// Memory reporter framework (owl:// pages).
BASE_DECLARE_FEATURE(kOwlMemoryReporter);

// Memory reporting interval in seconds (default: 5).
extern const base::FeatureParam<int> kOwlMemoryReportIntervalSeconds;

// Zombie compartment cleanup on page navigation/close.
BASE_DECLARE_FEATURE(kOwlZombieCleanup);

}  // namespace owl::features

#endif  // CHROME_BROWSER_OWL_OWL_FEATURE_FLAGS_H_
