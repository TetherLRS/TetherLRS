#!/usr/bin/env bash
# TetherLRS host tests: builds and runs every test_*.cpp here against the firmware sources, with the host's g++.
#   firmware/test/run_tests.sh
# The exit status is not 0 if a test does not build or fails.
set -u
cd "$(dirname "$0")"
OUT=$(mktemp -d)
status=0
for t in test_*.cpp; do
    if ! g++ -std=c++17 -Wall -I../mLRS/Common -I../mLRS/CommonTx "$t" -o "$OUT/${t%.cpp}"; then
        echo "$t: does not build"; status=1; continue
    fi
    if "$OUT/${t%.cpp}" > "$OUT/${t%.cpp}.log" 2>&1; then
        echo "$t: ok ($(grep -c -i "ok\|pass" "$OUT/${t%.cpp}.log") checks reported)"
    else
        echo "$t: FAILED"; cat "$OUT/${t%.cpp}.log"; status=1
    fi
done
rm -rf "$OUT"
exit $status
