#include <DiffRows.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
FileDiff sample()
{
    return parseDiff(R"(diff --git a/f b/f
--- a/f
+++ b/f
@@ -1,2 +1,2 @@
-a
+b
 c
@@ -10,1 +10,1 @@
-x
+y
)")
        .front();
}
} // namespace

auto tPlain = test("DiffRows/file header, hunk headers, then lines") = []
{
    auto rows = buildRows(sample(), {}, {}, 80);

    check(rows.size() == 8);
    check(rows[0].kind == RowKind::fileHeader);
    check(rows[1].kind == RowKind::hunkHeader && rows[1].hunk == 0);
    check(rows[2].kind == RowKind::line && rows[2].flat == 0);
    check(rows[5].kind == RowKind::hunkHeader && rows[5].hunk == 1);
    check(rows[7].flat == 4);
};

auto tBlockAfterAnchor = test("DiffRows/a comment block follows its last line") = []
{
    auto file = sample();
    auto annotation =
        makeAnnotation(file, {0, 1}, 7, "first\nsecond line that wraps");
    auto rows = buildRows(file, {annotation}, {}, 12);

    check(rows[3].flat == 1);
    check(rows[4].kind == RowKind::annotationTop && rows[4].annotation == 7);
    check(rows[5].kind == RowKind::annotationText && rows[5].text == "first");
    check(rows[6].text == "second line");
    check(rows[7].text == "that wraps");
    check(rows[8].kind == RowKind::commentThreadGap);
    check(rows[9].kind == RowKind::annotationBottom);
    check(rows[10].flat == 2);
};

auto tFileLevelFirst = test("DiffRows/file comments sit under the file header") = []
{
    auto file = sample();
    auto stale = makeAnnotation(file, {0, 0}, 3, "gone");
    stale.stale = true;
    auto whole = makeFileAnnotation("f", 4, "about the file");
    auto rows = buildRows(file, {stale, whole}, {}, 80);

    check(rows[0].kind == RowKind::fileHeader);
    check(rows[1].kind == RowKind::annotationTop && rows[1].annotation == 4);
    check(rows[5].kind == RowKind::annotationTop && rows[5].annotation == 3);
    check(rows[9].kind == RowKind::hunkHeader);
};

auto tBinary = test("DiffRows/a binary file is a header and one message row") = []
{
    auto rows =
        buildRows(diffForNewFile("x", std::string_view {"\0", 1}), {}, {}, 80);

    check(rows.size() == 2);
    check(rows[1].kind == RowKind::message);
};

auto tReplyRows = test("DiffRows/replies follow the comment inside its block") = []
{
    auto file = sample();
    auto annotation = makeAnnotation(file, {0, 0}, 7, "why?");
    annotation.replies = {{"dev", "2026-10-18 10:00", "because\nit is", false},
                          {"you", "2026-10-18 10:05", "ok", true}};
    auto rows = buildRows(file, {annotation}, {}, 80);

    auto isGap = [](const DiffRow& row)
    { return row.kind == RowKind::commentThreadGap; };

    check(rows[3].kind == RowKind::annotationTop);
    check(rows[4].text == "why?" && rows[4].reply == -1);
    check(isGap(rows[5]));
    check(rows[6].kind == RowKind::replyHeader && rows[6].reply == 0);
    check(rows[7].text == "because" && rows[7].reply == 0);
    check(rows[8].text == "it is");
    check(isGap(rows[9]));
    check(rows[10].kind == RowKind::replyHeader && rows[10].reply == 1);
    check(rows[11].text == "ok" && rows[11].reply == 1);
    check(isGap(rows[12]));
    check(rows[13].kind == RowKind::annotationBottom);
};

auto tCommentThreadCursor =
    test("DiffRows/a row in a thread is found again after a rebuild") = []
{
    auto file = sample();
    auto annotation = makeAnnotation(file, {0, 0}, 7, "why?");
    annotation.replies = {{"dev", "2026-10-18 10:00", "because\nit is", false}};
    auto rows = buildRows(file, {annotation}, {}, 80);
    constexpr auto secondReplyLine = std::size_t {8};
    constexpr auto bottomAfterRebuild = 13;
    constexpr auto farPastTheBottom = 99;

    check(rows[secondReplyLine].text == "it is");

    auto offset = findOffsetInCommentThread(rows, secondReplyLine);
    annotation.replies.push_back({"you", "2026-10-18 10:05", "ok", false});
    auto rebuilt = buildRows(file, {annotation}, {}, 80);

    check(findRowInCommentThread(rebuilt, 7, offset)
          == static_cast<int>(secondReplyLine));
    check(rebuilt[bottomAfterRebuild].kind == RowKind::annotationBottom);
    check(findRowInCommentThread(rebuilt, 7, farPastTheBottom)
          == bottomAfterRebuild);
    check(!findRowInCommentThread(rebuilt, 8, 0));
};

namespace
{
constexpr auto commentThreadId = 48;
constexpr auto wideEnough = 200;

Annotation makeCommentThreadWithReplies(const FileDiff& file, std::size_t count)
{
    auto annotation =
        makeAnnotation(file,
                       {0, 0},
                       commentThreadId,
                       "huh? you should have an Activity-ClaudeCode.h\nmore");

    for (std::size_t r = 0; r < count; ++r)
    {
        auto author = r % 2 == 0 ? std::string {"dev"} : std::string {"you"};
        annotation.replies.push_back(
            {author, "2026-10-18 10:00", "reply " + std::to_string(r), false});
    }

    return annotation;
}

std::vector<int> findShownReplies(const std::vector<DiffRow>& rows)
{
    auto shown = std::vector<int> {};

    for (const auto& row: rows)
    {
        if (row.kind == RowKind::replyHeader && row.reply >= 0)
            shown.push_back(row.reply);
    }

    return shown;
}

long countKind(const std::vector<DiffRow>& rows, RowKind kind)
{
    return std::count_if(rows.begin(),
                         rows.end(),
                         [&](const DiffRow& row) { return row.kind == kind; });
}
} // namespace

auto tRecentReplies = test(
    "DiffRows/a thread shows its last few replies under a row for the rest") = []
{
    constexpr auto replyCount = recentReplyCount + 2;
    auto file = sample();
    auto annotation = makeCommentThreadWithReplies(file, replyCount);
    auto rows = buildRows(file, {annotation}, {}, wideEnough);
    auto earlier = std::find_if(rows.begin(),
                                rows.end(),
                                [](const DiffRow& row)
                                { return row.kind == RowKind::earlierReplies; });

    check(earlier != rows.end());
    check(earlier->text == "⋯ 2 earlier replies · Enter to show all");
    check(findShownReplies(rows) == std::vector<int> {2, 3, 4, 5, 6});

    auto all = buildRows(file,
                         {annotation},
                         {{commentThreadId, {CommentThreadView::all, 0}}},
                         wideEnough);
    check(countKind(all, RowKind::earlierReplies) == 0);
    check(findShownReplies(all).size() == replyCount);

    auto few = makeCommentThreadWithReplies(file, recentReplyCount);
    check(countKind(buildRows(file, {few}, {}, wideEnough), RowKind::earlierReplies)
          == 0);
};

auto tCollapsedCommentThread =
    test("DiffRows/a collapsed thread is one summary row with what's new") = []
{
    constexpr auto repliesWhenCollapsed = std::size_t {3};
    constexpr auto narrow = 30;
    auto file = sample();
    auto annotation = makeCommentThreadWithReplies(file, repliesWhenCollapsed);
    auto folds = CommentThreadFolds {
        {commentThreadId, {CommentThreadView::collapsed, repliesWhenCollapsed}}};
    auto rows = buildRows(file, {annotation}, folds, wideEnough);

    check(countKind(rows, RowKind::collapsedCommentThread) == 1);
    check(countKind(rows, RowKind::annotationTop) == 0);
    check(countKind(rows, RowKind::annotationBottom) == 0);

    auto collapsed = std::find_if(rows.begin(), rows.end(), isCommentThreadStart);
    check(
        collapsed->text
        == "#48 · you: huh? you should have an Activity-ClaudeCode.h… · 3 replies");

    auto index = static_cast<std::size_t>(collapsed - rows.begin());
    check(findOffsetInCommentThread(rows, index) == 0);
    constexpr auto offsetPastTheSummary = 5;
    check(findRowInCommentThread(rows, commentThreadId, offsetPastTheSummary)
          == static_cast<int>(index));

    annotation.replies.push_back({"dev", "now", "done", false});
    annotation.replies.push_back({"you", "now", "thanks", false});
    auto fold = folds.at(commentThreadId);
    check(describeCollapsedCommentThread(annotation, fold, wideEnough)
              .ends_with(" · 5 replies · 1 new"));

    auto cut = describeCollapsedCommentThread(annotation, fold, narrow);
    check(cut.starts_with("#48 · you: ") && cut.find("…") != std::string::npos);
};

auto tCommentThreadFoldsFile =
    test("DiffRows/folds read back; the default isn't written") = []
{
    auto folds = CommentThreadFolds {{4, {CommentThreadView::collapsed, 3}},
                                     {5, {CommentThreadView::all, 0}},
                                     {6, {CommentThreadView::recent, 0}}};
    auto text = formatCommentThreadFolds(folds);
    auto parsed = parseCommentThreadFolds(text + "junk\n");

    check(text == "4 collapsed 3\n5 all\n");
    check(parsed.size() == 2);
    check(findFold(parsed, 4).view == CommentThreadView::collapsed
          && findFold(parsed, 4).repliesWhenCollapsed == 3);
    check(findFold(parsed, 5).view == CommentThreadView::all);
    check(findFold(parsed, 6).view == CommentThreadView::recent);
};
