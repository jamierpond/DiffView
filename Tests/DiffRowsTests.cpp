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
    auto rows = buildRows(sample(), {}, 80);

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
    auto rows = buildRows(file, {annotation}, 12);

    check(rows[3].flat == 1);
    check(rows[4].kind == RowKind::annotationTop && rows[4].annotation == 7);
    check(rows[5].kind == RowKind::annotationText && rows[5].text == "first");
    check(rows[6].text == "second line");
    check(rows[7].text == "that wraps");
    check(rows[8].kind == RowKind::annotationBottom);
    check(rows[9].flat == 2);
};

auto tFileLevelFirst = test("DiffRows/file comments sit under the file header") = []
{
    auto file = sample();
    auto stale = makeAnnotation(file, {0, 0}, 3, "gone");
    stale.stale = true;
    auto whole = makeFileAnnotation("f", 4, "about the file");
    auto rows = buildRows(file, {stale, whole}, 80);

    check(rows[0].kind == RowKind::fileHeader);
    check(rows[1].kind == RowKind::annotationTop && rows[1].annotation == 4);
    check(rows[4].kind == RowKind::annotationTop && rows[4].annotation == 3);
    check(rows[7].kind == RowKind::hunkHeader);
};

auto tBinary = test("DiffRows/a binary file is a header and one message row") = []
{
    auto rows = buildRows(diffForNewFile("x", std::string_view {"\0", 1}), {}, 80);

    check(rows.size() == 2);
    check(rows[1].kind == RowKind::message);
};
