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
    collapsedThread,
    earlierReplies
};

constexpr auto foldsFileName = "folds";

// Unfolded, a thread shows its comment and only this many of its last replies.
constexpr auto recentReplyCount = std::size_t {5};

enum class ThreadView
{
    collapsed,
    recent,
    all
};

struct ThreadFold
{
    ThreadView view = ThreadView::recent;
    // When it was collapsed: later replies from agents are new.
    std::size_t repliesWhenCollapsed = 0;
};

// By comment id; a thread not in it shows its recent replies.
using ThreadFolds = std::map<int, ThreadFold>;

std::string formatThreadFolds(const ThreadFolds& folds);
ThreadFolds parseThreadFolds(const std::string& text);

ThreadFold findFold(const ThreadFolds& folds, int id);

// "#48 · you: huh? you should… · 3 replies · 1 new", cut to fit `width`.
std::string describeCollapsedThread(const Annotation& annotation,
                                    const ThreadFold& fold,
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
                               const ThreadFolds& folds,
                               int wrapWidth);

// The first row of a thread, collapsed or not.
bool isThreadStart(const DiffRow& row);

int findOffsetInThread(const std::vector<DiffRow>& rows, std::size_t index);

std::optional<int>
    findRowInThread(const std::vector<DiffRow>& rows, int annotation, int offset);
} // namespace diffview
