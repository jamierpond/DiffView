#pragma once

#include "Annotations.h"
#include "Diff.h"

#include <cstddef>
#include <map>
#include <optional>
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
    annotationBottom,
    collapsedCommentThread,
    earlierReplies,
    commentThreadGap
};

constexpr auto foldsFileName = "folds";

// Unfolded, a thread shows its comment and only this many of its last replies.
constexpr auto recentReplyCount = std::size_t {5};

enum class CommentThreadView
{
    collapsed,
    recent,
    all
};

struct CommentThreadFold
{
    CommentThreadView view = CommentThreadView::recent;
    // When it was collapsed: later replies from agents are new.
    std::size_t repliesWhenCollapsed = 0;
};

// By comment id; a thread not in it shows its recent replies.
using CommentThreadFolds = std::map<int, CommentThreadFold>;

std::string formatCommentThreadFolds(const CommentThreadFolds& folds);
CommentThreadFolds parseCommentThreadFolds(const std::string& text);

CommentThreadFold findFold(const CommentThreadFolds& folds, int id);

// "#48 · you: huh? you should… · 3 replies · 1 new", cut to fit `width`.
std::string describeCollapsedCommentThread(const Annotation& annotation,
                                           const CommentThreadFold& fold,
                                           int width);

struct DiffRow
{
    RowKind kind = RowKind::line;
    int hunk = -1;
    int line = -1;

    // Index into flatLines(file).
    int flat = -1;

    int annotation = 0;

    std::string text;

    // -1: the comment itself.
    int reply = -1;
};

// File-level and stale comments go under the file header row; each line
// comment goes after its last line.
std::vector<DiffRow> buildRows(const FileDiff& file,
                               const std::vector<Annotation>& annotations,
                               const CommentThreadFolds& folds,
                               int wrapWidth);

// The first row of a thread, collapsed or not.
bool isCommentThreadStart(const DiffRow& row);

int findOffsetInCommentThread(const std::vector<DiffRow>& rows, std::size_t index);

std::optional<int> findRowInCommentThread(const std::vector<DiffRow>& rows,
                                          int annotation,
                                          int offset);
} // namespace diffview
