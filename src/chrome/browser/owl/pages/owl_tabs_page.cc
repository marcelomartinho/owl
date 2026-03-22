// Copyright 2026 Owl Browser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/owl/pages/owl_tabs_page.h"

#include <sstream>

namespace owl::pages {

// static
std::string OwlTabsPage::GenerateHtml() {
  std::ostringstream html;

  html << R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Owl Tabs</title>
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
      --blue: #4e9ff0;
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
    th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--border); }
    th { color: var(--text-muted); font-weight: 500; font-size: 13px; }
    .mono { font-family: 'SF Mono', 'Fira Code', monospace; }
    .status-active { color: var(--green); }
    .status-frozen { color: var(--blue); }
    .status-suspended { color: var(--yellow); }
    .status-discarded { color: var(--text-muted); }
    .badge {
      display: inline-block;
      padding: 2px 6px;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 500;
    }
    .badge-pin { background: var(--blue); color: white; }
    .badge-audio { background: var(--green); color: white; }
    .badge-webrtc { background: var(--yellow); color: black; }
    .badge-form { background: var(--accent); color: white; }
    button {
      background: var(--border);
      color: var(--text);
      border: none;
      padding: 4px 10px;
      border-radius: 4px;
      cursor: pointer;
      font-size: 12px;
    }
    button:hover { background: var(--accent); }
    button:disabled { opacity: 0.3; cursor: default; }
    .priority { width: 60px; }
    .priority-bar {
      height: 6px;
      border-radius: 3px;
      background: var(--green);
    }
    .priority-bar.low { background: var(--red); }
    .priority-bar.medium { background: var(--yellow); }
    .priority-bar.high { background: var(--green); }
    .summary { display: flex; gap: 24px; margin-bottom: 16px; }
    .summary-item { text-align: center; }
    .summary-value { font-size: 28px; font-weight: 700; color: var(--green); }
    .summary-label { font-size: 12px; color: var(--text-muted); }
  </style>
</head>
<body>
  <h1><span>owl://</span>tabs</h1>
  <p class="subtitle">Tab lifecycle manager - LRU priority & memory usage</p>

  <div class="card">
    <div class="summary">
      <div class="summary-item">
        <div class="summary-value" id="total-tabs">--</div>
        <div class="summary-label">Total Tabs</div>
      </div>
      <div class="summary-item">
        <div class="summary-value" id="frozen-tabs">--</div>
        <div class="summary-label">Frozen</div>
      </div>
      <div class="summary-item">
        <div class="summary-value" id="discarded-tabs">--</div>
        <div class="summary-label">Discarded</div>
      </div>
      <div class="summary-item">
        <div class="summary-value" id="protected-tabs">--</div>
        <div class="summary-label">Protected</div>
      </div>
      <div class="summary-item">
        <div class="summary-value" id="total-mem">--</div>
        <div class="summary-label">Total Memory</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>Tab Priority List</h2>
    <p style="color: var(--text-muted); font-size: 13px; margin-bottom: 12px;">
      Tabs sorted by discard priority. Low priority tabs are discarded first under memory pressure.
    </p>
    <table>
      <thead>
        <tr>
          <th>Priority</th>
          <th>Tab</th>
          <th>Status</th>
          <th>Protection</th>
          <th class="mono">Memory</th>
          <th class="mono">Idle Time</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody id="tab-list">
        <tr><td colspan="7" style="color: var(--text-muted)">
          Loading tab data...
        </td></tr>
      </tbody>
    </table>
  </div>

  <div class="card">
    <h2>Discard Policy</h2>
    <table>
      <tbody>
        <tr><td>Grace period</td><td class="mono">5 minutes</td></tr>
        <tr><td>Heavy tab threshold</td><td class="mono">11 tabs</td></tr>
        <tr><td>Max discard per event</td><td class="mono">3 tabs</td></tr>
        <tr><td>LRU weight</td><td class="mono">60%</td></tr>
        <tr><td>Memory weight</td><td class="mono">30%</td></tr>
        <tr><td>Current pressure tier</td>
          <td class="mono" id="current-tier">None</td></tr>
      </tbody>
    </table>
  </div>

  <script>
    // In actual implementation, communicates with OwlTabDiscardPolicy
    // via chrome.send() or Mojo to get real-time tab priority data.

    function freezeTab(tabId) {
      // chrome.send('freezeTab', [tabId]);
      console.log('Freeze tab:', tabId);
    }

    function discardTab(tabId) {
      // chrome.send('discardTab', [tabId]);
      console.log('Discard tab:', tabId);
    }

    function refresh() {
      // Fetch and update tab data.
    }

    setInterval(refresh, 5000);
  </script>
</body>
</html>)";

  return html.str();
}

}  // namespace owl::pages
