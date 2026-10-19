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
    test("Review/the monitor's line names the round, the counts and the file") = []
{
    check(roundNotice("2026-10-17-142300", {3, 0}, {}, "/r/.diffview/latest.md")
          == "dv: review round 2026-10-17-142300 submitted · 3 open · read "
             "/r/.diffview/latest.md");
    check(roundNotice("2026-10-17-142300", {1, 2}, "dev", "/r/.diffview/latest.md")
          == "dv: review round 2026-10-17-142300 submitted · for @dev · 1 open · "
             "2 with new replies · read /r/.diffview/latest.md");
};

namespace
{
Reply said(const std::string& author,
           const std::string& time,
           const std::string& text)
{
    return {author, time, text, false};
}
} // namespace

auto tMergeReplies =
    test("Review/replies are gathered from every version of a thread") = []
{
    auto first = comment(1, "q");
    first.replies = {said("dev", "2026-10-18 10:00", "a1")};
    auto second = comment(1, "q");
    second.replies = {said("you", "2026-10-18 10:05", "h1")};
    second.replies[0].fresh = true; // new in that round
    auto drafted = comment(1, "q");
    drafted.replies = {said("you", "2026-10-18 10:05", "h1"),
                       said("you", "2026-10-18 10:09", "h2")};

    auto reviews = mergeReviews({{"r1", {}, {}, {first}}, {"r2", {}, {}, {second}}},
                                {"draft", {}, {}, {drafted}});
    const auto& thread = reviews.annotations[0].replies;

    check(thread.size() == 3);
    check(thread[0].text == "a1" && !thread[0].fresh);
    check(thread[1].text == "h1" && !thread[1].fresh);
    check(thread[2].text == "h2" && thread[2].fresh);
};

auto tMergeOwner = test("Review/an owner, once set, is kept") = []
{
    auto claimed = comment(1, "q");
    claimed.owner = "dev";
    auto reviews = mergeReviews({{"r1", {}, {}, {claimed}}},
                                {"draft", {}, {}, {comment(1, "q2")}});

    check(reviews.annotations[0].owner == "dev");
    check(reviews.annotations[0].text == "q2");
};

auto tSubmitReplies =
    test("Review/a reply goes out marked new, with its thread, then isn't") = []
{
    auto sent = comment(1, "q");
    sent.replies = {said("dev", "2026-10-18 10:00", "a1")};
    auto drafted = sent;
    drafted.replies.push_back(said("you", "2026-10-18 10:05", "h1"));

    auto annotations =
        mergeReviews({{"r1", {}, {}, {sent}}}, {"draft", {}, {}, {drafted}})
            .annotations;
    auto round = submitDraft(annotations, "r2", {}, {});

    check(round.comments.size() == 1);
    check(round.comments[0].updates == "r1");
    check(round.comments[0].replies.size() == 2);
    check(!round.comments[0].replies[0].fresh && round.comments[0].replies[1].fresh);
    check(!annotations[0].replies[1].fresh);
};

auto tMentions = test("Review/@names in a text, not e-mail addresses") = []
{
    auto names = findMentions("@diffview-dev, look. Ask @b_2. not me@host.com (@c)");

    check(names.size() == 3);
    check(names[0] == "diffview-dev" && names[1] == "b_2" && names[2] == "c");

    auto note = comment(1, "for @dev");
    check(findOwner(note) == "dev");
    note.owner = "other";
    check(findOwner(note) == "other");
};

auto tRoundFor = test("Review/which rounds wake which agent") = []
{
    auto mine = comment(1, "@dev rename this");
    auto theirs = comment(2, "@ops fix the build");
    auto anyone = comment(3, "typo");
    auto replied = comment(4, "q");
    replied.owner = "ops";
    replied.replies = {
        said("you", "t", "and @dev, check this"),
    };
    replied.replies[0].fresh = true;
    replied.resolved = true;

    auto round = ReviewFile {"r", {}, {}, {mine, theirs, anyone, replied}};

    auto forDev = routeRound(round, "dev");
    check(forDev.open == 2 && forDev.replied == 1 && forDev.wakes());

    auto forOps = routeRound(round, "ops");
    check(forOps.open == 2 && forOps.replied == 1);

    auto everyone = routeRound(round, {});
    check(everyone.open == 3 && everyone.replied == 1);

    auto onlyOps = ReviewFile {"r", {}, {}, {theirs}};
    check(!routeRound(onlyOps, "dev").wakes());
    check(routeRound(onlyOps, "ops").wakes());

    auto resolvedAnyone = anyone;
    resolvedAnyone.resolved = true;
    check(!routeRound({"r", {}, {}, {resolvedAnyone}}, "dev").wakes());
};

auto tRoundsUpTo = test("Review/--after drops the rounds up to it") = []
{
    auto kept = keepRoundsUpTo(
        {"2026-10-16-100000.md", "2026-10-16-120000.md", "2026-10-16-120000-2.md"},
        "2026-10-16-120000");

    check(kept.size() == 2);
    check(kept[1] == "2026-10-16-120000.md");
};

auto tChangeThread = test("Review/reply and resolve claim, refuse, force") = []
{
    auto round =
        ReviewFile {"r", {}, {}, {comment(1, "q"), comment(2, "@ops look")}};

    auto change =
        ThreadChange {"dev", true, false, false, "on it", "2026-10-18 11:00"};
    check(!changeThread(round, 1, change));
    check(round.comments[0].owner == "dev");
    check(round.comments[0].replies.size() == 1);
    check(round.comments[0].replies[0].author == "dev");

    auto refused = changeThread(round, 2, change);
    check(refused && refused->find("@ops") != std::string::npos
          && refused->find("--force") != std::string::npos);
    check(round.comments[1].replies.empty());

    change.force = true;
    change.resolve = true;
    change.text.clear();
    check(!changeThread(round, 2, change));
    check(round.comments[1].resolved && round.comments[1].owner == "ops");
    check(round.comments[1].replies.empty());

    check(changeThread(round, 9, change).has_value());

    // Unnamed agents answer but don't claim.
    auto unnamed = ThreadChange {};
    unnamed.text = "x";
    auto open = ReviewFile {"r", {}, {}, {comment(3, "q")}};
    check(!changeThread(open, 3, unnamed));
    check(open.comments[0].owner.empty());
};

auto tResolutionsCarryReplies =
    test("Review/a reply written into latest.md goes into the round") = []
{
    auto round = ReviewFile {"r", {}, {}, {comment(1, "q")}};
    auto latest = round;
    latest.comments[0].replies = {said("dev", "t", "done")};

    auto updated = resolutionsFrom(latest, round);
    check(updated && updated->comments[0].replies.size() == 1);
    check(!resolutionsFrom(round, round));
};
