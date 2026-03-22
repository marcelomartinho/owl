#!/usr/bin/env bash
# Owl Browser - Memory Benchmark
# Compares memory usage between Owl and vanilla Chrome.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OWL_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="${CHROMIUM_DIR:-$OWL_ROOT/chromium}/src"
OWL_BINARY="${OWL_BINARY:-$CHROMIUM_SRC/out/Owl/chrome}"
CHROME_BINARY="${CHROME_BINARY:-google-chrome}"
RESULTS_DIR="$OWL_ROOT/benchmark_results"
IDLE_TIME="${IDLE_TIME:-300}"  # 5 minutes default

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[BENCH]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[BENCH]${NC} $*"; }
log_data()  { echo -e "${CYAN}[DATA]${NC} $*"; }

TEST_URLS=(
    "https://mail.google.com"
    "https://www.youtube.com"
    "https://www.reddit.com"
    "https://twitter.com"
    "https://github.com"
    "https://en.wikipedia.org/wiki/Main_Page"
    "https://www.cnn.com"
    "https://www.amazon.com"
    "https://stackoverflow.com"
    "https://docs.google.com"
)

measure_memory() {
    local browser_name="$1"
    local browser_pid="$2"

    # Get all child process PIDs (renderer, GPU, etc.)
    local all_pids
    all_pids=$(pgrep -P "$browser_pid" 2>/dev/null || echo "")
    all_pids="$browser_pid $all_pids"

    local total_rss=0
    local total_vss=0
    local process_count=0

    for pid in $all_pids; do
        if [ -f "/proc/$pid/status" ]; then
            local rss
            rss=$(awk '/VmRSS/ {print $2}' "/proc/$pid/status" 2>/dev/null || echo "0")
            local vss
            vss=$(awk '/VmSize/ {print $2}' "/proc/$pid/status" 2>/dev/null || echo "0")
            total_rss=$((total_rss + rss))
            total_vss=$((total_vss + vss))
            ((process_count++))
        fi
    done

    echo "$total_rss $total_vss $process_count"
}

run_benchmark() {
    local browser_name="$1"
    local browser_binary="$2"
    local result_file="$RESULTS_DIR/${browser_name}_$(date +%Y%m%d_%H%M%S).json"

    log_info "Starting benchmark for: $browser_name"
    log_info "Binary: $browser_binary"

    if [ ! -x "$browser_binary" ] && ! command -v "$browser_binary" &>/dev/null; then
        log_warn "Binary not found: $browser_binary (skipping)"
        return
    fi

    # Launch browser with test URLs
    "$browser_binary" \
        --no-first-run \
        --no-default-browser-check \
        --disable-sync \
        --user-data-dir="/tmp/owl_bench_${browser_name}_$$" \
        "${TEST_URLS[@]}" &
    local browser_pid=$!

    log_info "Browser PID: $browser_pid"
    log_info "Waiting 30s for pages to load..."
    sleep 30

    # Measure memory at intervals
    local measurements=()
    local interval=30
    local elapsed=0

    log_info "Measuring memory every ${interval}s for ${IDLE_TIME}s..."
    while [ "$elapsed" -lt "$IDLE_TIME" ]; do
        local mem_data
        mem_data=$(measure_memory "$browser_name" "$browser_pid")
        local rss=$(echo "$mem_data" | awk '{print $1}')
        local vss=$(echo "$mem_data" | awk '{print $2}')
        local procs=$(echo "$mem_data" | awk '{print $3}')

        local rss_mb=$((rss / 1024))
        local vss_mb=$((vss / 1024))
        log_data "$browser_name [${elapsed}s]: RSS=${rss_mb}MB VSS=${vss_mb}MB Processes=$procs"

        measurements+=("{\"time\": $elapsed, \"rss_kb\": $rss, \"vss_kb\": $vss, \"processes\": $procs}")

        sleep "$interval"
        elapsed=$((elapsed + interval))
    done

    # Generate JSON result
    local measurements_json
    measurements_json=$(printf '%s\n' "${measurements[@]}" | paste -sd ',' -)

    cat > "$result_file" <<EOF
{
    "browser": "$browser_name",
    "binary": "$browser_binary",
    "date": "$(date -Iseconds)",
    "idle_time_seconds": $IDLE_TIME,
    "test_urls_count": ${#TEST_URLS[@]},
    "measurements": [$measurements_json]
}
EOF

    log_info "Results saved to: $result_file"

    # Cleanup
    kill "$browser_pid" 2>/dev/null || true
    wait "$browser_pid" 2>/dev/null || true
    rm -rf "/tmp/owl_bench_${browser_name}_$$"
}

compare_results() {
    log_info ""
    log_info "=== Benchmark Comparison ==="

    local owl_last=$(ls -t "$RESULTS_DIR"/owl_*.json 2>/dev/null | head -1)
    local chrome_last=$(ls -t "$RESULTS_DIR"/chrome_*.json 2>/dev/null | head -1)

    if [ -z "$owl_last" ] || [ -z "$chrome_last" ]; then
        log_warn "Need both Owl and Chrome results for comparison."
        return
    fi

    # Extract final RSS measurements
    local owl_rss
    owl_rss=$(python3 -c "
import json
data = json.load(open('$owl_last'))
final = data['measurements'][-1]
print(final['rss_kb'] // 1024)
")
    local chrome_rss
    chrome_rss=$(python3 -c "
import json
data = json.load(open('$chrome_last'))
final = data['measurements'][-1]
print(final['rss_kb'] // 1024)
")

    local savings=$((chrome_rss - owl_rss))
    local pct=0
    if [ "$chrome_rss" -gt 0 ]; then
        pct=$((savings * 100 / chrome_rss))
    fi

    log_data "Chrome RSS: ${chrome_rss}MB"
    log_data "Owl RSS:    ${owl_rss}MB"
    log_data "Savings:    ${savings}MB (${pct}%)"

    if [ "$pct" -ge 30 ]; then
        log_info "TARGET MET: Owl uses ${pct}% less memory (goal: 30-40%)"
    else
        log_warn "TARGET NOT MET: Owl uses ${pct}% less memory (goal: 30-40%)"
    fi
}

main() {
    log_info "=== Owl Browser - Memory Benchmark ==="
    log_info "Test URLs: ${#TEST_URLS[@]} sites"
    log_info "Idle time: ${IDLE_TIME}s"
    echo ""

    mkdir -p "$RESULTS_DIR"

    run_benchmark "owl" "$OWL_BINARY"
    run_benchmark "chrome" "$CHROME_BINARY"
    compare_results

    log_info ""
    log_info "=== Benchmark complete ==="
    log_info "Results directory: $RESULTS_DIR"
}

main "$@"
