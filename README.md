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
| `j` `k` `gg` `G` | Move (`5j` works) | `Enter` `c` | Comment on the line (edits the one there) |
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
| `b` `:base` | Change base |  |  |
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
mode. `:w`
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
or withdrawn. An agent resolves a comment by changing its `· open` to
`· resolved` in the file it was given; DiffView picks that up.

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
Act on `open` comments; `resolved` and `withdrawn` ones need nothing. When you have dealt with one, change its `· open` to `· resolved` here.

## General

### General · open · #7

The search UX feels off overall.

## src/Focus.cpp

### src/Focus.cpp:48-49 · open, updated from review 2026-10-16-170000 · #4

```text
  46 │         return std::nullopt;
  47 │
     │-    found->second.since = nowMs;
> 48 │+    auto& spell = found->second;
> 49 │+    spell.since = nowMs;
  50 │ }
```

Why a reference here? It was a copy.
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

Tell an agent:

> I review your changes in DiffView. Read `.diffview/latest.md` and act on
> every `open` comment; the `>` lines in each block are the ones it is about.
> When you've dealt with one, change its `· open` to `· resolved` in that
> file. Don't edit anything else under `.diffview/`.

An agent that can watch a command's output (Claude Code's Monitor tool) can
run `dv monitor [repo]` to hear when you submit. It needs no terminal, prints
one line per round submitted after it starts, and runs until killed
(`--once`: until the first round):

```
dv: review round 2026-10-17-142300 submitted · 3 open · read /abs/repo/.diffview/latest.md
```

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
