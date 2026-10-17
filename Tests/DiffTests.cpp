#include <Diff.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto twoFiles = R"(diff --git a/src/Focus.cpp b/src/Focus.cpp
index 1111111..2222222 100644
--- a/src/Focus.cpp
+++ b/src/Focus.cpp
@@ -38,4 +45,5 @@ std::optional<std::string> FocusTracker::activated()
     if (found == spells.end())
         return std::nullopt;
-    found->second.since = nowMs;
+    auto& spell = found->second;
+    spell.since = nowMs;
 }
@@ -90,2 +98,1 @@ void tail()
-gone
 kept
diff --git a/new file.txt b/new file.txt
new file mode 100644
index 0000000..3333333
--- /dev/null
+++ b/new file.txt
@@ -0,0 +1,2 @@
+hello
+world
\ No newline at end of file
diff --git a/old.bin b/old.bin
deleted file mode 100644
index 4444444..0000000
Binary files a/old.bin and /dev/null differ
)";
} // namespace

auto tParsesFiles = test("Diff/parses each file with its status") = []
{
    auto files = parseDiff(twoFiles);

    check(files.size() == 3);
    check(files[0].path == "src/Focus.cpp");
    check(files[0].status == FileStatus::modified);
    check(files[1].path == "new file.txt");
    check(files[1].status == FileStatus::added);
    check(files[2].path == "old.bin");
    check(files[2].status == FileStatus::deleted);
    check(files[2].binary);
};

auto tParsesHunks = test("Diff/numbers lines on both sides") = []
{
    auto files = parseDiff(twoFiles);
    const auto& hunks = files[0].hunks;

    check(hunks.size() == 2);
    check(hunks[0].oldStart == 38 && hunks[0].newStart == 45);
    check(hunks[0].section
          == "std::optional<std::string> FocusTracker::activated()");
    check(hunks[0].lines.size() == 6);

    const auto& removed = hunks[0].lines[2];
    check(removed.kind == LineKind::removed);
    check(removed.oldLine == 40 && removed.newLine == 0);

    const auto& added = hunks[0].lines[4];
    check(added.kind == LineKind::added);
    check(added.newLine == 48 && added.oldLine == 0);
    check(added.text == "    spell.since = nowMs;");

    check(hunks[0].lines[5].oldLine == 41 && hunks[0].lines[5].newLine == 49);
    check(files[0].added == 2 && files[0].removed == 2);
};

auto tNoNewlineMarkerSkipped = test("Diff/skips the no-newline marker") = []
{
    auto files = parseDiff(twoFiles);

    check(files[1].hunks.size() == 1);
    check(files[1].hunks[0].lines.size() == 2);
    check(files[1].hunks[0].header() == "@@ -0,0 +1,2 @@");
};

auto tDashLinesInsideHunk = test("Diff/a removed '-- x' line is not a header") = []
{
    auto files = parseDiff(
        "diff --git a/x b/x\n--- a/x\n+++ b/x\n@@ -1,2 +1,1 @@\n--- a/y\n ok\n");

    check(files.size() == 1);
    check(files[0].hunks[0].lines.size() == 2);
    check(files[0].hunks[0].lines[0].text == "-- a/y");
    check(files[0].path == "x");
};

auto tQuotedPath = test("Diff/unquotes C-quoted paths") = []
{
    check(unquotePath("\"b/ta\\tb\"") == "b/ta\tb");
    check(unquotePath("\"caf\\303\\251\"") == "caf\xC3\xA9");
    check(unquotePath("plain") == "plain");

    auto files =
        parseDiff("diff --git \"a/ta\\tb\" \"b/ta\\tb\"\nnew file mode 100644\n");
    check(files.size() == 1 && files[0].path == "ta\tb");
};

auto tModeOnly = test("Diff/a mode-only change has no hunks") = []
{
    auto files = parseDiff(
        "diff --git a/run.sh b/run.sh\nold mode 100644\nnew mode 100755\n");

    check(files.size() == 1);
    check(files[0].path == "run.sh");
    check(files[0].hunks.empty());
};

auto tFingerprint = test("Diff/fingerprint follows content") = []
{
    auto a = diffForNewFile("a.txt", "one\ntwo\n");
    auto b = diffForNewFile("a.txt", "one\ntwo\n");
    auto c = diffForNewFile("a.txt", "one\nthree\n");

    check(a.fingerprint == b.fingerprint);
    check(a.fingerprint != c.fingerprint);
};

auto tNewFile = test("Diff/an untracked file is all additions") = []
{
    auto file = diffForNewFile("notes.md", "a\nb\nc");

    check(file.status == FileStatus::untracked);
    check(file.hunks.size() == 1);
    check(file.hunks[0].lines.size() == 3);
    check(file.hunks[0].lines[2].newLine == 3);
    check(file.added == 3);
    check(diffForNewFile("x.bin", std::string_view {"ab\0cd", 5}).binary);
};

auto tUnchangedFileDiff =
    test("Diff/an unchanged file is every line, unchanged") = []
{
    auto file = unchangedFile("a.txt", "one\ntwo\n");

    check(file.status == FileStatus::unchanged);
    check(file.hunks[0].lines[1].kind == LineKind::context);
    check(file.hunks[0].lines[1].oldLine == 2
          && file.hunks[0].lines[1].newLine == 2);
    check(file.added == 0 && file.removed == 0);
};
