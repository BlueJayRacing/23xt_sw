#!/usr/bin/env bash
# Runs everything that can be checked without hardware:
#   1. host unit tests (test/*_host)
#   2. every prod project as the app and as each test suite (SPI, CAN), warnings as errors
# Exits non-zero on the first failure. Run from anywhere.
set -euo pipefail

STM32_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "== host tests"
for t in "$STM32_DIR"/test/*_host; do
    [ -f "$t/CMakeLists.txt" ] || continue
    echo "-- $(basename "$t")"
    cmake -S "$t" -B "$t/build" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$t/build"
    ctest --test-dir "$t/build" --output-on-failure
done

echo "== firmware builds"
for p in "$STM32_DIR"/prod/*/; do
    [ -f "$p/CMakePresets.json" ] || continue
    name="$(basename "$p")"
    for variant in OFF SPI CAN; do
        echo "-- $name (test suite: $variant)"
        flags=(-DWERROR=ON -DMAG_BOARD_TEST_SUITE=$variant)
        (cd "$p" && cmake --preset Debug -B "build/check-$variant" "${flags[@]}" > /dev/null \
                 && cmake --build "build/check-$variant")
    done
done

echo "== all checks passed"
