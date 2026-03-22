// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/pages/owl_memory_page.h"

#include <sstream>

namespace owl::pages {

// static
std::string OwlMemoryPage::GenerateHtml() {
  std::ostringstream html;

  html << R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Owl Memory</title>
  <style>
    :root {
      --bg: #1a1a2e;
      --surface: #16213e;
      --border: #0f3460;
      --text: #e0e0e0;
      --text-muted: #888;
      --accent: #e94560;
      --green: #4ecca3;
      --yellow: #f0c040;
      --red: #e94560;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
      background: var(--bg);
      color: var(--text);
      padding: 24px;
      line-height: 1.5;
    }
    h1 { font-size: 24px; margin-bottom: 8px; }
    h1 span { color: var(--accent); }
    .subtitle { color: var(--text-muted); margin-bottom: 24px; }
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 16px;
      margin-bottom: 16px;
    }
    .card h2 { font-size: 16px; margin-bottom: 12px; color: var(--green); }
    table { width: 100%; border-collapse: collapse; }
    th, td {
      text-align: left;
      padding: 6px 12px;
      border-bottom: 1px solid var(--border);
    }
    th { color: var(--text-muted); font-weight: 500; font-size: 13px; }
    .bytes { font-family: 'SF Mono', 'Fira Code', monospace; text-align: right; }
    .bar {
      height: 8px;
      background: var(--green);
      border-radius: 4px;
      min-width: 2px;
    }
    .bar-bg {
      width: 120px;
      height: 8px;
      background: var(--border);
      border-radius: 4px;
    }
    .hog { color: var(--red); font-weight: bold; }
    .actions { display: flex; gap: 8px; margin-bottom: 24px; }
    button {
      background: var(--border);
      color: var(--text);
      border: none;
      padding: 8px 16px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 13px;
    }
    button:hover { background: var(--accent); }
    .total { font-size: 36px; font-weight: 700; color: var(--green); }
    .tier-none { color: var(--green); }
    .tier-warning { color: var(--yellow); }
    .tier-critical { color: var(--red); }
    .tier-emergency { color: var(--red); font-weight: bold; animation: pulse 1s infinite; }
    @keyframes pulse { 50% { opacity: 0.5; } }
    #status { color: var(--text-muted); font-size: 12px; }
  </style>
</head>
<body>
  <h1><span>owl://</span>memory</h1>
  <p class="subtitle">Hierarchical memory breakdown - refreshes every 5s</p>

  <div class="actions">
    <button onclick="forceGC()">Force GC</button>
    <button onclick="exportJson()">Export JSON</button>
    <button onclick="refresh()">Refresh Now</button>
    <span id="status">Loading...</span>
  </div>

  <div class="card">
    <h2>Total Memory</h2>
    <div class="total" id="total-memory">--</div>
    <p style="color: var(--text-muted); margin-top: 4px;">
      Pressure tier: <span id="pressure-tier" class="tier-none">None</span>
      &nbsp;|&nbsp; System RAM usage: <span id="system-usage">--</span>
    </p>
  </div>

  <div class="card">
    <h2>Per-Tab Memory</h2>
    <table>
      <thead>
        <tr><th>Tab</th><th>Status</th><th class="bytes">Memory</th><th>Usage</th></tr>
      </thead>
      <tbody id="tab-table">
        <tr><td colspan="4" style="color: var(--text-muted)">Collecting data...</td></tr>
      </tbody>
    </table>
  </div>

  <div class="card">
    <h2>Subsystem Breakdown</h2>
    <table>
      <thead>
        <tr><th>Subsystem</th><th class="bytes">Bytes</th><th>Share</th></tr>
      </thead>
      <tbody id="subsystem-table">
        <tr><td colspan="3" style="color: var(--text-muted)">Collecting data...</td></tr>
      </tbody>
    </table>
  </div>

  <div class="card">
    <h2>Extensions</h2>
    <table>
      <thead>
        <tr><th>Extension</th><th class="bytes">Memory</th><th>Status</th></tr>
      </thead>
      <tbody id="extension-table">
        <tr><td colspan="3" style="color: var(--text-muted)">Collecting data...</td></tr>
      </tbody>
    </table>
  </div>

  <div class="card">
    <h2>GC Statistics</h2>
    <table>
      <tbody id="gc-stats">
        <tr><td>Total GC cycles</td><td class="bytes" id="gc-cycles">--</td></tr>
        <tr><td>Background GC cycles</td><td class="bytes" id="gc-bg-cycles">--</td></tr>
        <tr><td>Last GC pause</td><td class="bytes" id="gc-last-pause">--</td></tr>
        <tr><td>Max GC pause</td><td class="bytes" id="gc-max-pause">--</td></tr>
        <tr><td>Total bytes reclaimed</td><td class="bytes" id="gc-reclaimed">--</td></tr>
        <tr><td>Zombie cleanups</td><td class="bytes" id="gc-zombies">--</td></tr>
      </tbody>
    </table>
  </div>

  <div class="card">
    <h2>Heap Unclassified</h2>
    <p>Memory not reported by any subsystem: <span id="unclassified" class="bytes">--</span></p>
    <p style="color: var(--text-muted); font-size: 13px;">
      A high value here indicates missing memory reporters.
    </p>
  </div>

  <script>
    // In the actual implementation, this page communicates with the
    // browser process via chrome.send() or a Mojo interface to get
    // real-time data from OwlMemoryMonitor.
    //
    // For now, it shows the page structure and layout.

    function refresh() {
      document.getElementById('status').textContent =
        'Last updated: ' + new Date().toLocaleTimeString();
    }

    function forceGC() {
      // chrome.send('forceGC');
      document.getElementById('status').textContent = 'GC requested...';
    }

    function exportJson() {
      // In actual implementation: fetch JSON from OwlMemoryMonitor.
      const data = { timestamp: Date.now(), note: 'placeholder' };
      const blob = new Blob([JSON.stringify(data, null, 2)], {type: 'application/json'});
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'owl_memory_' + Date.now() + '.json';
      a.click();
    }

    // Auto-refresh every 5 seconds.
    setInterval(refresh, 5000);
    refresh();
  </script>
</body>
</html>)";

  return html.str();
}

// static
std::string OwlMemoryPage::GenerateJson() {
  // In the actual implementation, this returns the JSON from
  // OwlMemoryMonitor::ExportAsJson().
  return R"({"status": "placeholder", "timestamp": 0})";
}

}  // namespace owl::pages
