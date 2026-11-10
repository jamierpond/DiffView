#!/usr/bin/env bash
# Counts the compile and link steps of each build, then times a clean build,
# a no-change rebuild and a one-file rebuild of the app.
# Usage: scripts/measure-build.sh <empty-build-dir> [parallel-jobs] [extra cmake args...]
set -euo pipefail

buildDirectory=${1:?"usage: $0 <empty-build-dir> [parallel-jobs] [extra cmake args...]"}
parallelJobs=${2:-6}
shift $(($# < 2 ? $# : 2))
sourceDirectory=$(cd "$(dirname "$0")/.." && pwd)
appTarget=DiffView
testTargets="DiffViewTests TextBufferTests VimEditorTests TelescopeTests"
fileTouchedForRebuild=Source/Render.cpp

rm -rf "$buildDirectory"
cmake -S "$sourceDirectory" -B "$buildDirectory" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release "$@" > /dev/null

countSteps()
{
    ninja -C "$buildDirectory" -n "$@" | grep -c '^\[' || true
}

timeAppBuild()
{
    local label=$1 startTime endTime
    startTime=$(date +%s.%N)
    cmake --build "$buildDirectory" --target $appTarget -j "$parallelJobs" > /dev/null
    endTime=$(date +%s.%N)
    echo "seconds, $label: $(awk "BEGIN { print $endTime - $startTime }")"
}

echo "steps, app (just build): $(countSteps $appTarget)"
echo "steps, bare cmake --build: $(countSteps)"
echo "steps, tests: $(countSteps $testTargets)"
echo "steps, just test: $(countSteps all)"

timeAppBuild "clean app build"
echo "steps, no-change rebuild: $(countSteps $appTarget)"
timeAppBuild "no-change rebuild"
touch "$sourceDirectory/$fileTouchedForRebuild"
echo "steps, one-file rebuild: $(countSteps $appTarget)"
timeAppBuild "one-file rebuild"
