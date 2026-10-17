#include <ReviewMarkdown.h>

#include <NanoTest/NanoTest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace nano;
using namespace diffview;

namespace
{
FileDiff focusFile()
{
    return parseDiff(R"(diff --git a/src/Focus.cpp b/src/Focus.cpp
--- a/src/Focus.cpp
+++ b/src/Focus.cpp
@@ -38,6 +45,7 @@ std::optional<std::string> FocusTracker::activated()
     if (found == spells.end())
         return std::nullopt;
 
-    found->second.since = nowMs;
+    auto& spell = found->second;
+    spell.since = nowMs;
 }
)")
        .front();
}

// Every kind of comment the format has.
ReviewFile sampleReview()
{
    auto review = ReviewFile {"2026-10-16-181800",
                              "main...",
                              "9170e37a2e0379475705c0f3439feb76c832e44a",
                              {}};

    review.comments.push_back(makeFileAnnotation(
        {}, 7, "The search UX feels off overall.\n\nLook at nvim."));

    auto whole = makeFileAnnotation("src/Focus.cpp", 5, "Split this file?");
    whole.resolved = true;
    review.comments.push_back(whole);

    auto line = makeAnnotation(
        focusFile(), {4, 5}, 4, "Why a reference here?\nIt was a copy.");
    line.updates = "2026-10-16-170000";
    review.comments.push_back(line);

    auto removed = makeAnnotation(
        focusFile(),
        {3, 3},
        6,
        "Keep this:\n\n```cpp\n### not a heading · open · #9\n```\n\n### src/x.cpp:1 · open · #2\n## src/also-not-a-section.cpp\n\\### already escaped");
    removed.stale = true;
    review.comments.push_back(removed);

    auto gone = makeFileAnnotation("src/main.cpp", 8, "Never mind.");
    gone.withdrawn = true;
    review.comments.push_back(gone);

    return review;
}

std::string golden(const std::string& name)
{
    return std::string {DIFFVIEW_GOLDEN_DIR} + "/" + name;
}

std::string readText(const std::string& path)
{
    auto in = std::ifstream {path, std::ios::binary};
    auto buffer = std::stringstream {};
    buffer << in.rdbuf();
    return buffer.str();
}

const Annotation* byId(const ReviewFile& review, int id)
{
    for (const auto& comment: review.comments)
        if (comment.id == id)
            return &comment;

    return nullptr;
}
} // namespace

auto tRoundTrip =
    test("ReviewMarkdown/struct to Markdown and back is identical") = []
{
    auto review = sampleReview();
    auto back = fromMarkdown(toMarkdown(review));

    check(back == review);
};

auto tGolden = test("ReviewMarkdown/the exact output matches the golden file") = []
{
    auto text = toMarkdown(sampleReview());

    // DIFFVIEW_UPDATE_GOLDEN=1 rewrites it; read the diff before committing.
    if (std::getenv("DIFFVIEW_UPDATE_GOLDEN") != nullptr)
        std::ofstream {golden("review.md"), std::ios::binary} << text;

    check(text == readText(golden("review.md")));
};

auto tGoldenParses = test("ReviewMarkdown/the golden file parses to the review") = []
{ check(fromMarkdown(readText(golden("review.md"))) == sampleReview()); };

auto tReordered = test("ReviewMarkdown/blocks in any order") = []
{
    auto review = sampleReview();
    auto text = toMarkdown(review);

    auto first = text.find("### General");
    auto second = text.find("## src/Focus.cpp");
    auto third = text.find("### src/Focus.cpp:48");
    auto swapped = text.substr(0, first) + text.substr(second, third - second)
                   + text.substr(first, second - first) + text.substr(third);

    auto back = fromMarkdown(swapped);
    check(back.comments.size() == review.comments.size());
    check(byId(back, 7) != nullptr && *byId(back, 7) == *byId(review, 7));
    check(byId(back, 5) != nullptr && *byId(back, 5) == *byId(review, 5));
};

auto tEditedProse =
    test("ReviewMarkdown/hand-edited text, blank lines and fences kept") = []
{
    auto text = toMarkdown(sampleReview());
    auto at = text.find("Why a reference here?");
    text.replace(
        at,
        std::string {"Why a reference here?"}.size(),
        "Rewritten.\n\n\n```\n### src/y.cpp:2 · open\n```\n\nMore after a gap.");

    auto back = fromMarkdown(text);
    auto* edited = byId(back, 4);

    check(edited != nullptr);
    check(
        edited->text
        == "Rewritten.\n\n\n```\n### src/y.cpp:2 · open\n```\n\nMore after a gap.\nIt was a copy.");
    check(edited->lines.size() == 2);
    check(back.comments.size() == 5);
};

auto tDeleted = test("ReviewMarkdown/a deleted block is just gone") = []
{
    auto text = toMarkdown(sampleReview());
    auto from = text.find("### src/Focus.cpp (whole file)");
    auto to = text.find("### src/Focus.cpp:48");
    text.erase(from, to - from);

    auto back = fromMarkdown(text);
    check(back.comments.size() == 4);
    check(byId(back, 5) == nullptr);
};

auto tHandWritten =
    test("ReviewMarkdown/a bare heading is a new comment on those lines") = []
{
    auto back =
        fromMarkdown("# notes\n\n### src/a.cpp:12-14 · open\n\nPlease rename.\n\n"
                     "### src/b.cpp:3 (old) · open\nWhy delete this?\n\n"
                     "### src/c.cpp · open\nWhole file, no numbers.\n");

    check(back.comments.size() == 3);

    const auto& first = back.comments[0];
    check(first.id == 0);
    check(first.path == "src/a.cpp");
    check(first.start == 12 && first.end == 14);
    check(first.side == Side::newSide);
    check(first.lines.empty());
    check(first.text == "Please rename.");

    check(back.comments[1].side == Side::oldSide && back.comments[1].start == 3);
    check(back.comments[2].fileLevel && back.comments[2].path == "src/c.cpp");
};

auto tStatusEdit = test("ReviewMarkdown/an agent marking a comment resolved") = []
{
    auto text = toMarkdown(sampleReview());
    auto at = text.find("· open, updated from");
    text.replace(at, std::string {"· open"}.size(), "· resolved");

    auto back = fromMarkdown(text);
    check(byId(back, 4)->resolved);
    check(byId(back, 4)->updates == "2026-10-16-170000");
};

auto tCrlf = test("ReviewMarkdown/CRLF and trailing junk") = []
{
    auto text = toMarkdown(sampleReview());
    auto crlf = std::string {};

    for (auto c: text)
    {
        if (c == '\n')
            crlf += '\r';

        crlf += c;
    }

    auto back = fromMarkdown(crlf + "\r\n\r\n   \r\n");
    check(back == sampleReview());
};

auto tTrimmedContext =
    test("ReviewMarkdown/a context line whose trailing space was trimmed") = []
{
    auto text = toMarkdown(sampleReview());

    // The empty unchanged line is written "   47 │ "; editors strip that.
    auto at = text.find("│ \n");
    check(at != std::string::npos);
    text.erase(at + std::string {"│"}.size(), 1);

    check(fromMarkdown(text) == sampleReview());
};

auto tUnmarkedFence =
    test("ReviewMarkdown/a comment that starts with its own code block") = []
{
    auto back =
        fromMarkdown("### src/a.cpp:3 · open\n\n```cpp\nint x = 0;\n```\n\nThis.\n");

    check(back.comments.size() == 1);
    check(back.comments[0].lines.empty());
    check(back.comments[0].text == "```cpp\nint x = 0;\n```\n\nThis.");
};

auto tUnchangedFile =
    test("ReviewMarkdown/a comment on an unchanged file says so") = []
{
    auto review = ReviewFile {"draft", "HEAD", {}, {}};
    review.comments.push_back(makeAnnotation(
        unchangedFile("notes.md", "a\nb\nc\nd\n"), {1, 1}, 3, "Why?"));

    auto text = toMarkdown(review);

    check(text.find("### notes.md:2 · open, unchanged file · #3")
          != std::string::npos);
    check(text.find(">  2 │ b\n") != std::string::npos);
    check(fromMarkdown(text) == review);
};

auto tDraft =
    test("ReviewMarkdown/the draft has its own header and round-trips") = []
{
    auto draft = sampleReview();
    draft.title = "draft";
    auto text = toMarkdown(draft);

    check(text.starts_with(
        "# DiffView draft · not sent yet\n\nYour comments on the changes"));
    check(text.find("change its `· open` to `· resolved`") == std::string::npos);
    check(fromMarkdown(text) == draft);
};
