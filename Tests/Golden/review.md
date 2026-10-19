# DiffView review · 2026-10-16-181800

Review comments on this repo's changes, diffed against `main...` at 9170e37a2e0379475705c0f3439feb76c832e44a.
Each `### path:lines · status · #id` heading is one comment: working-tree line numbers, `(old)` the base's, `(whole file)` the whole file.
In the block under it `>` marks the lines the comment is about (line number, then `+` added, `-` removed, space unchanged); the comment follows the block.
Replies follow a comment as `#### reply · author · time`: `you` is the reviewer, `new` marks theirs new in this round. `@name` after the status is the agent the thread belongs to.
Act on `open` comments; `resolved` and `withdrawn` ones need nothing. Answer with `dv reply <id> "text"`; when you have dealt with one, `dv resolve <id>` (or change its `· open` to `· resolved` here).

## General

### General · open · #7

The search UX feels off overall.

Look at nvim.

## src/Focus.cpp

### src/Focus.cpp (whole file) · resolved · #5

Split this file?

### src/Focus.cpp:48-49 · open, updated from review 2026-10-16-170000 · @diffview-dev · #4

```text
  46 │         return std::nullopt;
  47 │ 
     │-    found->second.since = nowMs;
> 48 │+    auto& spell = found->second;
> 49 │+    spell.since = nowMs;
  50 │ }
```

Why a reference here?
It was a copy.

#### reply · diffview-dev · 2026-10-16 17:10

It aliases the map entry.

#### reply · you · 2026-10-16 18:15 · new

Then say so:

\#### reply · not one
```
#### reply · nor this
```

### src/Focus.cpp:41 (old) · open, stale: its lines have left the diff · #6

```text
  38 │     if (found == spells.end())
  39 │         return std::nullopt;
  40 │ 
> 41 │-    found->second.since = nowMs;
     │+    auto& spell = found->second;
     │+    spell.since = nowMs;
  42 │ }
```

Keep this:

```cpp
### not a heading · open · #9
```

\### src/x.cpp:1 · open · #2
\## src/also-not-a-section.cpp
\### already escaped

## src/main.cpp

### src/main.cpp (whole file) · withdrawn · #8

Never mind.
