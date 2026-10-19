#include <Annotations.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
FileDiff fileFrom(const std::string& diffText)
{
    return parseDiff(diffText).front();
}

const auto original = std::string {R"(diff --git a/f.cpp b/f.cpp
--- a/f.cpp
+++ b/f.cpp
@@ -10,5 +10,6 @@ void f()
 one
 two
-old
+new
+newer
 three
)"};

// Two lines added above the hunk: everything moves down by two.
const auto shifted = std::string {R"(diff --git a/f.cpp b/f.cpp
--- a/f.cpp
+++ b/f.cpp
@@ -1,0 +2,2 @@
+top1
+top2
@@ -10,5 +12,6 @@ void f()
 one
 two
-old
+new
+newer
 three
)"};
} // namespace

auto tMake = test("Annotations/anchors a range with context") = []
{
    auto file = fileFrom(original);
    auto annotation = makeAnnotation(file, {3, 4}, 1, "why?");

    check(annotation.path == "f.cpp");
    check(annotation.side == Side::newSide);
    check(annotation.start == 12 && annotation.end == 13);
    check(annotation.lines.size() == 2);
    check(annotation.lines[0] == "+new");
    check(annotation.before.size() == 3);
    check(annotation.before[2] == "-old");
    check(annotation.after.size() == 1 && annotation.after[0] == " three");
    check(lineLabel(annotation) == "12-13");
};

auto tRemovedOnly = test("Annotations/a removed line anchors on the old side") = []
{
    auto file = fileFrom(original);
    auto annotation = makeAnnotation(file, {2, 2}, 1, "");

    check(annotation.side == Side::oldSide);
    check(annotation.start == 12);
    check(lineLabel(annotation) == "12 (old)");
};

auto tShift = test("Annotations/follows its lines when the diff shifts") = []
{
    auto annotation = makeAnnotation(fileFrom(original), {3, 4}, 1, "why?");
    auto moved = reanchor(annotation, fileFrom(shifted));

    check(moved);
    check(!annotation.stale);
    check(annotation.start == 14 && annotation.end == 15);
    check(annotation.text == "why?");

    auto range = findAnchor(annotation, fileFrom(shifted));
    check(range && range->first == 5 && range->last == 6);
};

auto tUnchanged =
    test("Annotations/re-anchoring on the same diff changes nothing") = []
{
    auto file = fileFrom(original);
    auto annotation = makeAnnotation(file, {3, 4}, 1, "why?");

    check(!reanchor(annotation, file));
};

auto tStale =
    test("Annotations/goes stale when its lines vanish, and comes back") = []
{
    auto annotation = makeAnnotation(fileFrom(original), {3, 3}, 1, "x");
    auto gone = fileFrom("diff --git a/f.cpp b/f.cpp\n--- a/f.cpp\n+++ b/f.cpp\n"
                         "@@ -10,1 +10,1 @@\n-a\n+b\n");

    check(reanchor(annotation, gone));
    check(annotation.stale);
    check(!reanchor(annotation, gone));

    check(reanchor(annotation, fileFrom(original)));
    check(!annotation.stale);
};

auto tEnds =
    test("Annotations/a range holds on by its ends when its middle changes") = []
{
    auto annotation = makeAnnotation(fileFrom(original), {0, 5}, 1, "block");
    auto edited = fileFrom(R"(diff --git a/f.cpp b/f.cpp
--- a/f.cpp
+++ b/f.cpp
@@ -10,5 +10,7 @@ void f()
 one
 two
-old
+new
+inserted
+newer
 three
)");

    check(reanchor(annotation, edited));
    check(!annotation.stale);
    check(annotation.lines.size() == 7);
    check(annotation.start == 10 && annotation.end == 15);
};

auto tContextWins =
    test("Annotations/duplicate lines are told apart by context") = []
{
    auto file = fileFrom(R"(diff --git a/f.cpp b/f.cpp
--- a/f.cpp
+++ b/f.cpp
@@ -1,3 +1,4 @@
 alpha
+return;
 beta
@@ -20,3 +21,4 @@
 gamma
+return;
 delta
)");

    auto annotation = makeAnnotation(file, {4, 4}, 1, "second one");
    check(annotation.start == 22);

    // Shift the second hunk; the first "return;" is closer to the old line
    // number but has the wrong context.
    auto moved = fileFrom(R"(diff --git a/f.cpp b/f.cpp
--- a/f.cpp
+++ b/f.cpp
@@ -1,3 +1,4 @@
 alpha
+return;
 beta
@@ -20,3 +41,4 @@
 gamma
+return;
 delta
)");

    reanchor(annotation, moved);
    check(annotation.start == 42);
};

auto tFileGone =
    test("Annotations/a file leaving the diff makes its notes stale") = []
{
    auto annotations =
        std::vector<Annotation> {makeAnnotation(fileFrom(original), {3, 3}, 1, "")};

    check(reanchorAll(annotations, {}));
    check(annotations[0].stale);
    check(nextAnnotationId(annotations) == 2);
};

auto tUnchangedLookup =
    test("Annotations/comments on unchanged files stay anchored") = []
{
    auto whole = unchangedFile("notes.md", "a\nb\nc\n");
    auto annotation = makeAnnotation(whole, {1, 1}, 1, "here");
    check(annotation.unchangedFile);
    check(annotation.start == 2);

    auto annotations = std::vector<Annotation> {annotation};
    auto lookup = [&](const std::string& path)
    { return path == "notes.md" ? OptionalFileDiff {whole} : OptionalFileDiff {}; };

    check(!reanchorAll(annotations, {}, lookup));
    check(!annotations[0].stale);

    check(reanchorAll(annotations, {}));
    check(annotations[0].stale);
};
