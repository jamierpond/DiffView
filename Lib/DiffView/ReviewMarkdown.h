#pragma once

#include "Annotations.h"

#include <Miro/Reflect.h>

#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
// The draft or a submitted round. The Markdown is the only copy, written for
// an agent to read and a person to edit by hand:
//
//   # DiffView review · 2026-10-16-180900
//
//   <four lines saying how to read it>
//
//   ### src/Focus.cpp:46-47 · open · #4
//
//   ```text
//      44 │     if (found == spells.end())
//      45 │         return std::nullopt;
//   >     │-    found->second.since = nowMs;
//   >  46 │+    auto& spell = found->second;
//   >  47 │+    spell.since = nowMs;
//      48 │ }
//   ```
//
//   Why hold a reference here? It was a copy before.
//
//   #### reply · diffview-dev · 2026-10-18 14:02
//
//   It aliases the map entry; a copy would be lost.
//
// The heading is the only metadata ("path:12 (old)" for removed lines,
// "path (whole file)", "General"); the context block is what re-anchors the
// comment when the code moves. By hand, a heading alone is enough: the
// comment is anchored by its line numbers. `@@` is avoided on purpose: it
// reads as a hunk header.
struct ReviewFile
{
    // "draft", or the round's id.
    std::string title;
    std::string baseLabel;
    std::string baseSha;
    std::vector<Annotation> comments;

    // One reply on one thread, sent the moment it was written.
    bool reply = false;

    bool operator==(const ReviewFile&) const = default;

    MIRO_REFLECT(title, baseLabel, baseSha, comments, reply)
};

// A Markdown frontend for Miro, as its XML one: Markdown to and from Miro's
// value tree, so the reflected structs load and save through Miro. Fields
// the format doesn't carry load as their defaults.
std::string printMarkdown(const Miro::JSON& review);
Miro::JSON parseMarkdown(std::string_view text);

std::string toMarkdown(const ReviewFile& review);
ReviewFile fromMarkdown(std::string_view text);
std::string printComment(const Annotation& comment);
} // namespace diffview
