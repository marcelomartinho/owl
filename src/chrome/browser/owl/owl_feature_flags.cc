// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/owl_feature_flags.h"

namespace owl::features {

BASE_FEATURE(kOwlMemoryOptimization,
             "OwlMemoryOptimization",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOwlAggressiveGC,
             "OwlAggressiveGC",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kOwlBackgroundGCIntervalSeconds{
    &kOwlAggressiveGC, "background_gc_interval_seconds", 5};

const base::FeatureParam<int> kOwlMaxGCPauseMs{
    &kOwlAggressiveGC, "max_gc_pause_ms", 10};

BASE_FEATURE(kOwlIsoHeap,
             "OwlIsoHeap",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOwlArenaAllocator,
             "OwlArenaAllocator",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kOwlArenaChunkSize{
    &kOwlArenaAllocator, "arena_chunk_size", 65536};

BASE_FEATURE(kOwlAdBlocker,
             "OwlAdBlocker",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kOwlAdBlockerUseFlatBuffers{
    &kOwlAdBlocker, "use_flatbuffers", true};

BASE_FEATURE(kOwlCustomUI,
             "OwlCustomUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOwlMemoryPressureTiers,
             "OwlMemoryPressureTiers",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double> kOwlPressureWarningThreshold{
    &kOwlMemoryPressureTiers, "warning_threshold", 0.80};

const base::FeatureParam<double> kOwlPressureCriticalThreshold{
    &kOwlMemoryPressureTiers, "critical_threshold", 0.90};

const base::FeatureParam<double> kOwlPressureEmergencyThreshold{
    &kOwlMemoryPressureTiers, "emergency_threshold", 0.95};

BASE_FEATURE(kOwlTabLRUUnloading,
             "OwlTabLRUUnloading",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kOwlTabUnloadGracePeriodSeconds{
    &kOwlTabLRUUnloading, "tab_unload_grace_period_seconds", 300};

const base::FeatureParam<int> kOwlHeavyTabDetectionThreshold{
    &kOwlTabLRUUnloading, "heavy_tab_detection_threshold", 11};

BASE_FEATURE(kOwlMemoryReporter,
             "OwlMemoryReporter",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kOwlMemoryReportIntervalSeconds{
    &kOwlMemoryReporter, "memory_report_interval_seconds", 5};

BASE_FEATURE(kOwlZombieCleanup,
             "OwlZombieCleanup",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace owl::features
