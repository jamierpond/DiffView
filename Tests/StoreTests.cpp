#include <Git.h>
#include <Store.h>
#include <Text.h>

#include <eacp/Core/Utils/Files.h>

#include <NanoTest/NanoTest.h>

#include <filesystem>
#include <fstream>

using namespace nano;
using namespace diffview;

namespace
{
namespace fs = std::filesystem;

std::string runGit(const std::string& repo, std::vector<std::string> args)
{
    return git::run(git::command(repo, std::move(args))).value_or("");
}

// main/ with a commit, and main-perf/ as a linked worktree on its own branch.
fs::path makeRepoWithWorktree(const std::string& name)
{
    auto dir = fs::temp_directory_path() / "DiffViewTests" / name;
    fs::remove_all(dir);
    fs::create_directories(dir / "main");
    dir = fs::canonical(dir);

    auto main = (dir / "main").string();
    runGit(main, {"init", "-q", "-b", "main"});
    runGit(main, {"config", "user.email", "test@example.com"});
    runGit(main, {"config", "user.name", "Test"});
    std::ofstream {dir / "main" / "a.txt"} << "one\n";
    runGit(main, {"add", "."});
    runGit(main, {"commit", "-q", "-m", "first"});
    runGit(main,
           {"worktree", "add", "-q", "-b", "perf", (dir / "main-perf").string()});

    return dir;
}

Annotation makeComment(int id, const std::string& text)
{
    auto comment = Annotation {};
    comment.id = id;
    comment.path = "a.txt";
    comment.fileLevel = true;
    comment.text = text;
    comment.draft = false;
    return comment;
}

void writeRound(const fs::path& worktree, const ReviewFile& round)
{
    writeText(
        eacp::FilePath {worktree / ".diffview" / "reviews" / (round.title + ".md")},
        toMarkdown(round));
}

std::optional<Annotation> findComment(const std::vector<ReviewFile>& rounds,
                                      const std::string& text)
{
    for (const auto& round: rounds)
    {
        for (const auto& comment: round.comments)
        {
            if (comment.text == text)
                return comment;
        }
    }

    return std::nullopt;
}
} // namespace

auto tWorktreeList = test("Store/parseWorktreeList reads git's porcelain") = []
{
    auto entries =
        parseWorktreeList("worktree /r/main\nHEAD abc\nbranch refs/heads/main\n"
                          "\nworktree /r/perf\nHEAD def\ndetached\n");

    check(entries.size() == 2);
    check(entries[0].path == "/r/main" && !entries[0].bare);
    check(entries[1].path == "/r/perf");

    auto bare = parseWorktreeList("worktree /r/repo.git\nbare\n\nworktree /r/wt\n");
    check(bare[0].bare && !bare[1].bare);
};

auto tLayoutFromBothWorktrees = test(
    "Store/one store for the repo, found from the main and a linked worktree") = []
{
    auto dir = makeRepoWithWorktree("StoreLayout");

    auto fromMain = findStore((dir / "main").string());
    auto fromLinked = findStore((dir / "main-perf").string());

    check(fromMain.has_value() && fromLinked.has_value());
    check(fromMain->directory.str() == (dir / "main" / ".diffview").string());
    check(fromLinked->directory == fromMain->directory);
    check(fromMain->worktreeName.empty());
    check(fromMain->worktreeDirectory == fromMain->directory);
    check(fromLinked->worktreeName == "main-perf");
    check(fromLinked->worktreeDirectory.str()
          == (dir / "main" / ".diffview" / "worktrees" / "main-perf").string());
    check(fromLinked->worktree.str() == (dir / "main-perf").string());
    check(fromMain->linkedWorktrees.size() == 1);
    check(fromMain->linkedWorktrees[0] == (dir / "main-perf").string());
};

auto tBareLayout =
    test("Store/a bare repo keeps the store in its git directory") = []
{
    auto layout = calculateStoreLayout("/r/wt",
                                       "/r/repo.git/worktrees/wt",
                                       "/r/repo.git",
                                       {{"/r/repo.git", true}, {"/r/wt", false}});

    check(layout.directory.str() == "/r/repo.git/.diffview");
    check(layout.worktreeName == "wt");
    check(layout.linkedWorktrees.size() == 1);
};

auto tMerge =
    test("Store/linked worktrees' stores merge in, colliding ids renumbered") = []
{
    auto dir = makeRepoWithWorktree("StoreMerge");
    auto main = dir / "main";
    auto perf = dir / "main-perf";

    auto mainRound = ReviewFile {
        .title = "2026-10-16-100000",
        .baseLabel = "HEAD",
        .baseSha = "abc",
        .comments = {makeComment(1, "main one"), makeComment(2, "main two")}};
    writeRound(main, mainRound);

    auto perfRound = ReviewFile {
        .title = "2026-10-16-100000",
        .baseLabel = "HEAD",
        .baseSha = "def",
        .comments = {makeComment(1, "perf one"), makeComment(5, "perf five")}};
    perfRound.comments[0].text = "perf one ![image](.diffview/images/1-1.png)";
    writeRound(perf, perfRound);

    auto perfLater = ReviewFile {.title = "2026-10-17-090000",
                                 .baseLabel = "HEAD",
                                 .baseSha = "def",
                                 .comments = {makeComment(3, "perf three")}};
    writeRound(perf, perfLater);
    writeText(eacp::FilePath {perf / ".diffview" / "latest.md"},
              toMarkdown(perfLater));
    writeText(eacp::FilePath {perf / ".diffview" / "images" / "1-1.png"}, "png");
    writeText(eacp::FilePath {perf / ".diffview" / "seen"}, "1 2\n5 1\n");

    auto draft = ReviewFile {.title = "draft",
                             .baseLabel = "HEAD",
                             .baseSha = "def",
                             .comments = {makeComment(2, "perf draft two")}};
    draft.comments[0].draft = true;
    writeText(eacp::FilePath {perf / ".diffview" / "draft.md"}, toMarkdown(draft));

    auto store = findStore(perf.string());
    check(store.has_value());

    auto notes = mergeWorktreeStores(*store);
    check(notes.size() == 1);
    check(notes[0].find("#1 is now #6") != std::string::npos);
    check(!fs::exists(perf / ".diffview"));

    auto ids = listRoundIdsInOrder(store->directory);
    check(ids.size() == 3);
    check(ids[0] == "2026-10-16-100000" && ids[1] == "2026-10-16-100000-2");

    auto rounds = readRounds(*store);
    check(rounds[0].worktree == main.string());
    check(rounds[1].worktree == perf.string());
    check(rounds[1].renumbering.find("was round 2026-10-16-100000")
          != std::string::npos);
    check(rounds[1].renumbering.find("#1 is now #6") != std::string::npos);

    auto perfOne =
        findComment(rounds, "perf one ![image](.diffview/images/6-1.png)");
    check(perfOne && perfOne->id == 6);
    check(eacp::Files::exists(main / ".diffview" / "images" / "6-1.png"));
    check(findComment(rounds, "perf five")->id == 5);
    check(findComment(rounds, "main one")->id == 1);
    check(findComment(rounds, "perf three")->id == 3);

    auto movedDraft = readReviewOrEmpty(store->worktreeDirectory / draftFileName);
    check(movedDraft.comments.size() == 1 && movedDraft.comments[0].id == 7);

    auto seen = eacp::Files::readFile(store->worktreeDirectory / seenFileName);
    check(seen == "6 2\n5 1\n");

    auto latest = readReviewOrEmpty(store->directory / latestReviewFileName);
    check(latest.title == "2026-10-17-090000");

    check(mergeWorktreeStores(*store).empty());
};

auto tRoundTagging = test("Store/a round records its worktree and base") = []
{
    auto round = ReviewFile {.title = "2026-10-18-120000",
                             .baseLabel = "main...",
                             .baseSha = "abc123",
                             .comments = {makeComment(4, "x")}};
    round.worktree = "/r/main-perf";
    round.renumbering = "#1 is now #6";

    auto parsed = fromMarkdown(toMarkdown(round));
    check(parsed.worktree == "/r/main-perf");
    check(parsed.renumbering == "#1 is now #6");
    check(parsed.baseLabel == "main..." && parsed.baseSha == "abc123");

    auto merged = mergeReviews({parsed}, {});
    check(merged.annotations[0].worktree == "/r/main-perf");
};
