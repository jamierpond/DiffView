#include <Review.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
Annotation comment(int id, const std::string& text)
{
    auto note = makeFileAnnotation("a.cpp", id, text);
    return note;
}
} // namespace

auto tMergeNewest = test("Review/the draft's version of a comment wins") = []
{
    auto first = ReviewFile {
        "2026-10-16-100000", {}, {}, {comment(1, "v1"), comment(2, "x")}};
    auto second = ReviewFile {"2026-10-16-110000", {}, {}, {comment(1, "v2")}};
    auto draft =
        ReviewFile {"draft", {}, {}, {comment(2, "edited"), comment(3, "new")}};

    auto reviews = mergeReviews({first, second}, draft);

    check(reviews.rounds.size() == 2);
    check(reviews.annotations.size() == 3);
    check(reviews.annotations[0].text == "v2");
    check(reviews.annotations[0].round == "2026-10-16-110000");
    check(!reviews.annotations[0].draft);
    check(reviews.annotations[1].text == "edited");
    check(reviews.annotations[1].draft);
    check(reviews.annotations[1].round == "2026-10-16-100000");
    check(reviews.annotations[2].round.empty());
    check(!reviews.renumbered);
};

auto tMergeIds = test("Review/comments written without an id get new ones") = []
{
    auto round = ReviewFile {"r", {}, {}, {comment(4, "sent")}};
    auto draft =
        ReviewFile {"draft", {}, {}, {comment(0, "by hand"), comment(0, "also")}};

    auto reviews = mergeReviews({round}, draft);

    check(reviews.renumbered);
    check(reviews.annotations.size() == 3);
    check(reviews.annotations[1].id == 5);
    check(reviews.annotations[2].id == 6);
};

auto tWithdrawn = test("Review/a sent withdrawal is gone, a drafted one isn't") = []
{
    auto gone = comment(1, "x");
    gone.withdrawn = true;
    auto pending = comment(2, "y");
    pending.withdrawn = true;

    auto reviews =
        mergeReviews({{"r", {}, {}, {gone}}}, {"draft", {}, {}, {pending}});

    check(reviews.annotations.size() == 1);
    check(reviews.annotations[0].id == 2);
};

auto tSubmit = test("Review/submitting freezes the draft into a round") = []
{
    auto annotations = mergeReviews({{"r1", {}, {}, {comment(1, "old")}}},
                                    {"draft", {}, {}, {comment(2, "new")}})
                           .annotations;
    annotations[0].text = "unchanged";
    annotations[0].draft = false;

    check(draftCount(annotations) == 1);

    auto round = submitDraft(annotations, "r2", "main...", "abc");

    check(round.title == "r2");
    check(round.baseLabel == "main...");
    check(round.comments.size() == 1 && round.comments[0].id == 2);
    check(draftCount(annotations) == 0);
    check(annotations[1].round == "r2");
};

auto tSubmitWithdrawn =
    test("Review/a withdrawal goes out once, then disappears") = []
{
    auto note = comment(1, "x");
    note.round = "r1";
    note.withdrawn = true;
    auto annotations = std::vector<Annotation> {note};

    auto round = submitDraft(annotations, "r2", {}, {});

    check(round.comments.size() == 1 && round.comments[0].withdrawn);
    check(annotations.empty());
};

auto tDraftOrder =
    test("Review/the draft lists files in order, whole-file first") = []
{
    auto line = makeFileAnnotation("b.cpp", 1, "line");
    line.fileLevel = false;
    line.start = 5;
    auto file = makeFileAnnotation("b.cpp", 2, "file");
    auto other = makeFileAnnotation("a.cpp", 3, "a");

    auto draft = draftFile({line, file, other}, "HEAD", {});

    check(draft.comments.size() == 3);
    check(draft.comments[0].id == 3);
    check(draft.comments[1].id == 2);
    check(draft.comments[2].id == 1);
};

auto tRoundId = test("Review/round ids are sortable timestamps") = []
{
    auto id = roundId(1760000000000);

    check(id.size() == 17);
    check(id[4] == '-' && id[7] == '-' && id[10] == '-');
    check(roundLabel("2026-10-16-180959") == "10-16 18:09");
};

auto tOpenCount = test("Review/open counts neither resolved nor withdrawn") = []
{
    auto resolved = comment(2, "b");
    resolved.resolved = true;
    auto withdrawn = comment(3, "c");
    withdrawn.withdrawn = true;

    check(openCount({comment(1, "a"), resolved, withdrawn, comment(4, "d")}) == 2);
};

auto tNewRounds =
    test("Review/the monitor reports only unseen rounds, oldest first") = []
{
    auto seen = std::vector<std::string> {"2026-10-16-100000.md"};
    auto current = std::vector<std::string> {
        "2026-10-17-090000.md", "2026-10-16-100000.md", "2026-10-16-120000.md"};

    auto fresh = newRounds(seen, current);

    check(fresh.size() == 2);
    check(fresh[0] == "2026-10-16-120000.md");
    check(fresh[1] == "2026-10-17-090000.md");
    check(newRounds(current, current).empty());
};

auto tRoundNotice =
    test("Review/the monitor's line names the round and the file") = []
{
    check(roundNotice("2026-10-17-142300", 3, "/r/.diffview/latest.md")
          == "dv: review round 2026-10-17-142300 submitted · 3 open · read "
             "/r/.diffview/latest.md");
};
