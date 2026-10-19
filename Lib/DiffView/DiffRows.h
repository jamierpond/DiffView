#pragma once

#include "Annotations.h"
#include "Diff.h"

#include <string>
#include <vector>

namespace diffview
{
enum class RowKind
{
    fileHeader,
    message,
    hunkHeader,
    line,
    annotationTop,
    annotationText,
    replyHeader,
    annotationBottom
};

struct DiffRow
{
    RowKind kind = RowKind::line;
    int hunk = -1;
    int line = -1;

    // Index into flatLines(file).
    int flat = -1;

    int annotation = 0;

    std::string text;

    // Which reply a replyHeader or text row belongs to; -1 is the comment
    // itself, whose header shows only once it has replies.
    int reply = -1;
};

// File-level and stale comments go under the file header row; each line
// comment goes after its last line.
std::vector<DiffRow> buildRows(const FileDiff& file,
                               const std::vector<Annotation>& annotations,
                               int wrapWidth);
} // namespace diffview
