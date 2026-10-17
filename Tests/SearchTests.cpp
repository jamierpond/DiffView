#include <Search.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

auto tGrep = test("Search/grep finds lines across files' diffs") = []
{
    auto files = parseDiff(R"(diff --git a/a b/a
--- a/a
+++ b/a
@@ -1,2 +1,2 @@
-old Needle
+new needle
 other
diff --git a/b b/b
--- a/b
+++ b/b
@@ -5,1 +5,1 @@
-x
+needle again
)");

    auto hits = grepDiffs(files, "needle");
    check(hits.size() == 3);
    check(hits[0].path == "a" && hits[0].flat == 0 && hits[0].lineNumber == 1);
    check(hits[1].line == "+new needle");
    check(hits[2].path == "b" && hits[2].flat == 1 && hits[2].lineNumber == 5);
    check(grepDiffs(files, "Needle").size() == 1);
};
