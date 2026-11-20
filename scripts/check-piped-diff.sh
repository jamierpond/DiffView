#!/usr/bin/env bash
# Pipes a diff into dv in a throwaway tmux session and saves what it drew after
# each key, so the piped-diff mode can be checked without a person at the
# keyboard: inside <repo> (comments work), outside any git work tree (read-only),
# and with nothing on standard input (dv must refuse).
#
# usage: scripts/check-piped-diff.sh <diffview binary> <repo with history> <out dir>
set -euo pipefail

binary=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
repo=$(cd "$2" && pwd)
mkdir -p "$3"
out=$(cd "$3" && pwd)
session=dv-check-piped-diff
width=150
height=40
start_wait_seconds=2
key_wait_seconds=1

capture() {
    tmux capture-pane -p -t "$session" > "$out/$1.txt"
}

press() {
    tmux send-keys -t "$session" "$1"
    sleep "$key_wait_seconds"
    capture "$2"
}

start() {
    tmux kill-session -t "$session" 2>/dev/null || true
    tmux new-session -d -s "$session" -x "$width" -y "$height" -c "$1" \
        "$2; echo \"exit \$?\"; sleep 60"
    sleep "$start_wait_seconds"
    capture "$3"
}

start "$repo" "git -c color.ui=always diff HEAD~3 | '$binary'" in-repo-start
press j in-repo-after-j
press l in-repo-after-l
press b in-repo-after-b
press f in-repo-after-f
press a in-repo-after-a
press r in-repo-after-r
press q in-repo-after-q

outside=$(mktemp -d)
git -C "$repo" diff HEAD~3 > "$outside/change.patch"
start "$outside" "'$binary' < change.patch" outside-start
press l outside-after-l
press c outside-after-c
press q outside-after-q
rm -rf "$outside"

start "$repo" "'$binary' < /dev/null" empty-input
tmux kill-session -t "$session"

echo "Saved to $out"
