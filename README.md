<h1 align="center">DiffView</h1>

<p align="center"><b>Watch your coding agents work in a live terminal diff, and tell them what you think, line by line.</b></p>

Point it at a repo an agent is editing (or just run `dv` inside it). The left pane is a tree of every
changed file, the right pane the selected file's diff. It refreshes as files
change. Files you haven't looked at since they last changed get a dot, and
files touched in the last ten seconds are highlighted, so you can see where
the agent is working. Comment on a line, a range, a hunk or a whole file, then
submit them as a review round, like a GitHub PR review: the round is written
to `.diffview/latest.md`, a plain Markdown file the agent reads.

## Run

```sh
just run                        # on this repo
just run ~/projects/other       # on another one
just run --base main...         # diff against where HEAD forked from main
dv main...                       # same, installed; a non-directory argument is the base
```

The base defaults to `HEAD`: the working tree and the index against the last
commit, untracked files included. `b` changes it. `name...` means the
merge-base of HEAD and `name`, as in `git diff main...`; anything else
(`main`, a sha, `HEAD~3`) is that exact commit.

## Keys

| Key | Action | Key | Action |
|---|---|---|---|
| `j` `k` `gg` `G` | Move (`5j` works) | `Enter` `c` | Comment on the line (`c` edits the one there; `Enter` replies if it has replies) |
| `^D` `^U` | Half page | `V` … `c` | Comment on a range |
| `h` `l` `Tab` | Tree / diff / switch | `gh` `C` | Comment on the hunk / whole file |
| `Enter` `l` (tree) | Toggle directory, open file | `gc` `:note` | General note, about no file |
| `]c` `[c` | Next / previous hunk | `dd` `x` | Delete (withdraw, if sent) / resolve |
| `]f` `[f` `^N` `^P` | Next / previous file | `X` `:resolved` | Show / hide resolved comments |
| `]u` `[u` | Next / previous unseen file | `S` `:submit` | Submit the draft as a review round |
| `]a` `[a` | Next / previous comment | `e` | Edit the file in `$EDITOR` at this line |
| `/` `?` `n` `N` | Search (incremental, smartcase) | `o` | Open the file with the OS |
| `Space pf` | Find a changed file | `F1` `g?` | Help |
| `Space ps` | Grep every changed line | `q` `:q` | Quit (asks about an unsent draft) |
| `Space pc` | Every comment, newest first; Enter amends | `:` | Commands (Tab completes): `submit q base comments note resolved edit help` |
| `b` `:base` | Change base | `R` | Reply on the comment's thread |
| `r` | Refresh | `a` | All files / changed files only |
| | | `s` | Tree / flat list by size of change |

`a` shows every file in the repo, not just the changed ones: an unchanged
file opens whole, and you comment on it as on a diff (its review block says
"unchanged file"). `s` flattens the tree into a list sorted by the size of
each change. `Space pf` follows both.

`/` and `?` move to the match as you type; Esc goes back to where you were,
Enter stays. `n`/`N` repeat it and centre the line. `/` in the tree searches
file names.

The comment box is a small vim: it opens in insert mode for a new comment and
normal mode for an existing one. `i a o O I A`, `h j k l w b e 0 $ gg G`,
`x dd dw cw ciw cc C D yy p P J r u ^R`, `/ ? n N`, and `^W ^U ^H` in insert
mode; `Tab` after `@` completes an agent's name. `:w`
saves, `:wq` or `:x` (or `^S`) saves and closes, `:q` closes, `:q!` throws the
edit away. Long lines wrap, and `j`/`k` move by wrapped line. `^V` pastes; on
macOS an image on the clipboard (a screenshot) is saved to
`.diffview/images/<comment>-<n>.png` and linked into the comment, and it is
deleted with the comment.

## Review rounds and what agents read

Comments go into a draft, `.diffview/draft.md`. `S` (or `:submit`) freezes it
into a round, `.diffview/reviews/<YYYY-MM-DD-HHMMSS>.md`, and copies that to
`.diffview/latest.md`. Quitting with a draft asks whether to submit it first
(`n` keeps it for next time).

Like a GitHub review: sent comments stay in the diff tagged "submitted
HH:MM"; resolved ones fold into one "✓ N resolved" line (`X` shows them).
Editing, resolving or deleting a sent comment puts that change in the draft,
and the next round says it is an update ("updated from review …"), resolved,
or withdrawn.

Each comment is a thread. Agents answer with `dv reply` and close with `dv
resolve`; their replies show under the comment as they arrive, each message
under its author (`you` in yellow, each agent in a colour of its own). `R`
(or `Enter` on a thread with replies) replies, and saving sends it at once,
no `S`: it becomes a one-thread round, `.diffview/reviews/<id>.md` headed
`# DiffView reply · <id>`, holding the whole thread with your reply marked
`new`, and wakes the agent the thread belongs to (every agent, if nobody's).
latest.md stays the last full round. New comments still wait for `S`.

A resolved thread whose newest reply is an agent's stays open in the diff
until the cursor has been on it (`.diffview/seen` remembers), so an answer
isn't folded away before you read it.

`@name` in a comment gives its thread to that agent, and the owner is shown
after the status. An unaddressed thread is everyone's until an agent
answers it, which claims it. An agent can't answer another's thread without
`--force`. Agents waiting with `dv monitor --as <name>` show in the status
bar (`◉ @name`).

### The files are the store

There is nothing else: the Markdown files are what DiffView loads, and you
can edit them by hand while it runs (fix a comment, delete a block, add one);
it reloads them. A comment you write by hand needs only its heading,
`### src/a.cpp:12-14 · open`: it is anchored on those lines and given an id.
A whole review looks like this:

````markdown
# DiffView review · 2026-10-16-181800

Review comments on this repo's changes, diffed against `main...` at 9170e37a2e0379475705c0f3439feb76c832e44a.
Each `### path:lines · status · #id` heading is one comment: working-tree line numbers, `(old)` the base's, `(whole file)` the whole file.
In the block under it `>` marks the lines the comment is about (line number, then `+` added, `-` removed, space unchanged); the comment follows the block.
Replies follow a comment as `#### reply · author · time`: `you` is the reviewer, `new` marks theirs new in this round. `@name` after the status is the agent the thread belongs to.
Act on `open` comments; `resolved` and `withdrawn` ones need nothing. Answer with `dv reply <id> "text"`. `dv resolve <id>` only once the work is done; a question gets a reply and stays open.

## General

### General · open · #7

The search UX feels off overall.

## src/Focus.cpp

### src/Focus.cpp:48-49 · open, updated from review 2026-10-16-170000 · @diffview-dev · #4

```text
  46 │         return std::nullopt;
  47 │
     │-    found->second.since = nowMs;
> 48 │+    auto& spell = found->second;
> 49 │+    spell.since = nowMs;
  50 │ }
```

Why a reference here? It was a copy.

#### reply · diffview-dev · 2026-10-16 17:10

It aliases the map entry.

#### reply · you · 2026-10-16 18:15 · new

Then say so in a name.
````

- One `###` heading per comment: where (path and working-tree lines; `(old)`
  for base-side lines of removed code; `(whole file)`; `General` for notes
  about no file), the status, the id. Nothing else is hidden anywhere.
- The block under it is the diff around the comment, `>` on the lines it is
  about. It is what re-anchors the comment when the code moves (by the
  lines' text and three lines of context either side; if they leave the
  diff the comment is marked stale).
- The comment is plain Markdown after the block, up to the next heading;
  code blocks and blank lines in it are fine.
- Replies follow it, each under `#### reply · author · YYYY-MM-DD HH:MM`
  (`you` is the reviewer; `· new` marks yours new in this round).
- `@name` between the status and the id is the agent the thread belongs to.

Tell an agent:

> I review your changes in DiffView. Run `dv --help` and follow its agent
> loop, as `--as <your name>`.

### The agent side

No terminal needed; every command takes `--repo <dir>` (default: the repo
around the working directory) and `dv <command> --help` has an example.

| Command | Does |
|---|---|
| `dv monitor --once --as <name> --after <round>` | Wait for a round with something for you; print one line and exit |
| `dv comments [--open] [--as <name>]` | One line per thread: id, status, owner, where, first line |
| `dv show <id>` | One thread: the code block, the comment, every reply |
| `dv reply <id> [-m] "<text>"` | Answer (text from stdin if not given) |
| `dv resolve <id> ["<note>"]` | Resolve, with an optional closing note |

The loop: run the `monitor` as a background shell task, so it costs nothing
while nobody reviews (a watcher tool that times out would be re-armed all
night). When it exits it prints the round:

```
dv: review round 2026-10-17-142300 submitted · for @dev · 3 open · 1 with new replies · read /abs/repo/.diffview/latest.md
dv: reply 2026-10-17-143512 on #4 from you · for @dev · read /abs/repo/.diffview/reviews/2026-10-17-143512.md
```

The second is a reply sent on its own; `dv show 4` shows the thread.
Resolve a thread only once the work is done. A question gets a reply and
stays open: resolving folds the thread away from the reviewer.

Read the threads (`dv comments --open --as dev`, `dv show <id>`), act,
`dv reply`/`dv resolve`, and run the monitor again with `--after` set to
the id in the line (`2026-10-17-142300`). A round already newer than `--after` exits it at once,
so nothing slips in between. With `--as`, rounds holding nothing for you
(only other agents' threads) are skipped; without it every round counts.
Plain `dv monitor` still streams a line per round until killed.

`.diffview` has its own `.gitignore` (`*`), so none of it shows up in `git
status`, the diff, or an agent's `git add -A`. Images pasted into comments
live in `.diffview/images/`. Files from older versions (`annotations.md`,
`annotations.json`) are imported once and kept, renamed.

## Build

Needs CMake 3.31+, Ninja, [just](https://github.com/casey/just) and a C++20
compiler.

```sh
just build     # build/diffview
just test      # unit tests
just install   # ~/.local/bin/diffview, plus dv
```

DiffView is built on [eacp](https://github.com/eyalamirmusic/eacp)'s Core
(the app lifecycle, its event loop and timers, child processes) and
[FTXUI](https://github.com/ArthurSonzogni/FTXUI) for the terminal UI. eacp
owns the main thread; a 16 ms eacp timer pumps FTXUI one non-blocking step at
a time, git runs on eacp's process threads and lands back on the main thread,
so nothing is shared between threads. The repo is watched with FSEvents on
macOS and polled once a second elsewhere. Work against a local eacp checkout
with `-DCPM_eacp_SOURCE=$HOME/projects/eacp`.

Nothing under `Lib/` draws; each library has its own tests in `Tests/`:

- `Lib/TextBuffer`: lines of text, edits, undo, and smartcase search (also as
  free functions over any lines, which is how the diff pane's `/` works).
- `Lib/VimEditor`: the comment box's vim, on a TextBuffer.
- `Lib/Telescope`: the pickers' model: fuzzy matching, the filtered list,
  selection, the query line.
- `Lib/DiffView`: diff parsing, the tree, comments and their anchors, review
  rounds and their Markdown (a Markdown frontend for
  [Miro](https://github.com/eyalamirmusic/Miro): Markdown to and from Miro's
  value tree, the reflected structs through Miro), seen-tracking, key
  sequences.

`Source/` is the app: the eacp/FTXUI shell, input, drawing.
