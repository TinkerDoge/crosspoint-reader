#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/reading_stats"
BINARY="$BUILD_DIR/ReadingStatsAnalyticsTest"

mkdir -p "$BUILD_DIR"

SOURCES=(
  "$ROOT_DIR/test/reading_stats/ReadingStatsAnalyticsTest.cpp"
  "$ROOT_DIR/src/util/ReadingStatsAnalytics.cpp"
)

CXXFLAGS=(
  -std=c++20
  -O2
  -Wall
  -Wextra
  -pedantic
  -I"$ROOT_DIR/src"
  -I"$ROOT_DIR/src/util"
  -I"$ROOT_DIR"
)

c++ "${CXXFLAGS[@]}" "${SOURCES[@]}" -o "$BINARY"

"$BINARY" "$@"
