#!/usr/bin/env bash
# Runs dv in a throwaway tmux session at each terminal size worth checking and
# saves what it drew, as plain text and with colours, so a layout can be looked
# at without a phone. At each size it also taps the first entry of the file
# list, the way a phone terminal reports a tap (an SGR mouse press and release).
#
# usage: scripts/capture-layouts.sh <diffview binary> <repo with changes> <out dir>
set -euo pipefail

binary=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
repo=$2
out=$3
session=dv-capture-layouts
sizes="45x35 45x25 30x35 59x35 60x35 80x35 150x40"
start_wait_seconds=2
tap_wait_seconds=1

# Row 3, column 10: the first file-list entry under the two-row page header.
first_entry_tap=$'\e[<0;10;3M\e[<0;10;3m'

mkdir -p "$out"

capture() {
    tmux capture-pane -p -t "$session" > "$out/$1.txt"
    tmux capture-pane -p -e -t "$session" > "$out/$1.ansi"
}

for size in $sizes; do
    width=${size%x*}
    height=${size#*x}
    tmux kill-session -t "$session" 2>/dev/null || true
    tmux new-session -d -s "$session" -x "$width" -y "$height" -c "$repo" \
        "'$binary'; sleep 60"
    sleep "$start_wait_seconds"
    capture "$size"
    tmux send-keys -t "$session" -l "$first_entry_tap"
    sleep "$tap_wait_seconds"
    capture "$size-after-tap"
    tmux kill-session -t "$session"
done

echo "Saved to $out"
