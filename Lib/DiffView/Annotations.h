#pragma once

#include "Diff.h"

#include <Miro/Reflect.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
enum class Side
{
    newSide,
    oldSide
};

// One message in a comment's thread. `you` is the reviewer, anything else an
// agent's name. `fresh`: not sent yet (in memory and the draft), or first
// sent in this round (in a round file); only the reviewer's are ever fresh.
struct Reply
{
    std::string author;
    std::string time; // "2026-10-18 14:02", local
    std::string text;
    bool fresh = false;

    bool operator==(const Reply&) const = default;

    MIRO_REFLECT(author, time, text, fresh)
};

constexpr auto reviewerName = "you";

// Anchored by the text of its lines plus a little context either side, so
// it finds them again when the diff shifts. Diff lines are kept as git
// prints them, '+', '-' or ' ' first.
struct Annotation
{
    int id = 0;
    std::string path;

    // With an empty path: a general note, about no file.
    bool fileLevel = false;

    // The base's numbers only for a run of removed lines.
    Side side = Side::newSide;
    int start = 0;
    int end = 0;

    std::vector<std::string> before;
    std::vector<std::string> lines;
    std::vector<std::string> after;

    std::string text;
    bool resolved = false;

    // The agent the thread belongs to: named with @ in the comment, or the
    // first to answer it. Empty: anyone's.
    std::string owner;

    std::vector<Reply> replies;

    bool withdrawn = false;

    // On a file with no changes (shown whole), so its lines have no +/-.
    bool unchangedFile = false;

    // The earlier round this changes, so the round says "updated from".
    std::string updates;

    // Its lines left the diff; it re-attaches if they come back.
    bool stale = false;

    // Which file it came from, so never written into one.
    std::string round;
    bool draft = true;

    bool operator==(const Annotation&) const = default;

    MIRO_REFLECT(id,
                 path,
                 fileLevel,
                 side,
                 start,
                 end,
                 before,
                 lines,
                 after,
                 text,
                 resolved,
                 owner,
                 replies,
                 withdrawn,
                 unchangedFile,
                 updates,
                 stale,
                 round,
                 draft)
};

// Indices into flatLines(): the hunks laid end to end.
struct LineRange
{
    int first = 0;
    int last = 0;
};

struct FlatLine
{
    int hunk = 0;
    int line = 0;
};

std::vector<FlatLine> flatLines(const FileDiff& file);

constexpr auto contextLines = 3;

Annotation
    makeAnnotation(const FileDiff& file, LineRange range, int id, std::string text);

Annotation makeFileAnnotation(const std::string& path, int id, std::string text);

// The run whose text matches (else whose first and last lines do), ranked
// by matching context, then by distance from the old line numbers.
std::optional<LineRange> findAnchor(const Annotation& annotation,
                                    const FileDiff& file);

// One written by hand with only line numbers is anchored on them.
bool reanchor(Annotation& annotation, const FileDiff& file);

// `unchanged` finds the files that aren't in the diff (nullptr if gone).
bool reanchorAll(
    std::vector<Annotation>& annotations,
    const std::vector<FileDiff>& files,
    const std::function<const FileDiff*(const std::string&)>& unchanged =
        [](const std::string&) -> const FileDiff* { return nullptr; });

// The .diffview/images/ files the text links to.
std::vector<std::string> imagesIn(const std::string& text);

int nextAnnotationId(const std::vector<Annotation>& annotations);

std::string lineLabel(const Annotation& annotation);

// The @names in a text, in order: "@diffview-dev, look" names diffview-dev.
// Not an e-mail address; a trailing '.' is the sentence's.
std::vector<std::string> mentionsIn(const std::string& text);

// Its owner, else the first agent its text names.
std::string ownerOf(const Annotation& annotation);
} // namespace diffview
