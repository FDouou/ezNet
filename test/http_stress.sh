#!/bin/bash
# TODO: 完善压测脚本

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
CONCURRENCY="${3:-10}"
REQUESTS="${4:-1000}"
URL="${5:-/hello}"

echo "=== LightNet HTTP Stress Test ==="
echo "Target: http://${HOST}:${PORT}${URL}"
echo "Concurrency: ${CONCURRENCY}"
echo "Total Requests: ${REQUESTS}"
echo ""

if command -v ab &> /dev/null; then
    ab -n "${REQUESTS}" -c "${CONCURRENCY}" "http://${HOST}:${PORT}${URL}"
elif command -v wrk &> /dev/null; then
    wrk -c "${CONCURRENCY}" -d 10s -t 4 "http://${HOST}:${PORT}${URL}"
elif command -v curl &> /dev/null; then
    echo "Using curl for simple test..."
    for i in $(seq 1 5); do
        curl -s -o /dev/null -w "Request %{time_total}s %{http_code}\n" "http://${HOST}:${PORT}${URL}"
    done
else
    echo "Error: Need ab (apache bench), wrk, or curl installed."
    exit 1
fi

echo ""
echo "=== Test Complete ==="
