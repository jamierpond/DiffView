#include <Diff.h>
#include <Git.h>
#include <Text.h>

#include <NanoTest/NanoTest.h>

#include <filesystem>
#include <fstream>

using namespace nano;
using namespace diffview;

namespace
{
std::string freshRepo(const std::string& name)
{
    auto dir = std::filesystem::temp_directory_path() / "DiffViewTests" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    auto repo = dir.string();

    git::run(git::command(repo, {"init", "-q", "-b", "main"}));
    git::run(git::command(repo, {"config", "user.email", "test@example.com"}));
    git::run(git::command(repo, {"config", "user.name", "Test"}));

    std::ofstream {dir / "tracked.txt"} << "one\ntwo\nthree\n";
    git::run(git::command(repo, {"add", "."}));
    git::run(git::command(repo, {"commit", "-q", "-m", "first"}));

    return repo;
}

void write(const std::string& repo, const std::string& path, const std::string& text)
{
    std::ofstream {std::filesystem::path {repo} / path} << text;
}
} // namespace

auto tBaseSpec = test("Git/BaseSpec reads the merge-base spelling") = []
{
    check(git::BaseSpec::parse("main...").mergeBase);
    check(git::BaseSpec::parse("main...").ref == "main");
    check(!git::BaseSpec::parse("abc123").mergeBase);
    check(git::BaseSpec::parse("  ").ref == "HEAD");
    check(git::BaseSpec::parse("main...").label() == "main...");
};

auto tCommits = test("Git/parseCommits") = []
{
    auto commits = git::parseCommits("abc1234\tFirst thing\ndef5678\tSecond\tone\n");

    check(commits.size() == 2);
    check(commits[1].sha == "def5678" && commits[1].subject == "Second\tone");
};

auto tChoices = test("Git/baseChoices offers merge-base and tip per branch") = []
{
    auto choices =
        git::baseChoices({"main", "origin/HEAD", "origin/main"}, {{"abc", "x"}});

    check(choices.size() == 6);
    check(choices[0].value == "HEAD");
    check(choices[1].value == "main...");
    check(choices[2].value == "main");
    check(choices[3].value == "origin/main...");
    check(choices[5].value == "abc");
};

auto tWorkingTree = test("Git/diffs the working tree and lists untracked files") = []
{
    auto repo = freshRepo("working-tree");
    write(repo, "tracked.txt", "one\nTWO\nthree\n");
    write(repo, "new file.txt", "hello\n");

    auto sha = git::run(git::resolveBase(repo, {}));
    check(sha.has_value());

    auto files = parseDiff(*git::run(git::diff(repo, trim(*sha))));
    check(files.size() == 1);
    check(files[0].path == "tracked.txt");
    check(files[0].added == 1 && files[0].removed == 1);

    auto untracked = splitNul(*git::run(git::untracked(repo)));
    check(untracked.size() == 1 && untracked[0] == "new file.txt");
};

auto tMergeBase = test("Git/resolves a merge-base") = []
{
    auto repo = freshRepo("merge-base");
    auto mainSha = trim(*git::run(git::resolveBase(repo, {})));

    git::run(git::command(repo, {"checkout", "-q", "-b", "feature"}));
    write(repo, "tracked.txt", "changed\n");
    git::run(git::command(repo, {"commit", "-q", "-am", "feature work"}));

    auto base = git::run(git::resolveBase(repo, git::BaseSpec::parse("main...")));
    check(base && trim(*base) == mainSha);

    check(!git::run(git::resolveBase(repo, git::BaseSpec::parse("nope"))));
};

auto tRoot = test("Git/findRoot") = []
{
    auto repo = freshRepo("root");
    std::filesystem::create_directories(std::filesystem::path {repo} / "sub");

    auto root = git::findRoot(repo + "/sub");
    check(root.has_value());
    check(std::filesystem::equivalent(*root, repo));
};

auto tAffectsDiff = test("Git/only HEAD, the index and refs count inside .git") = []
{
    check(git::affectsDiff("src/a.cpp"));
    check(git::affectsDiff(".gitignore"));
    check(git::affectsDiff(".git/HEAD"));
    check(git::affectsDiff(".git/index"));
    check(git::affectsDiff(".git/refs/heads/main"));
    check(!git::affectsDiff(".git/index.lock"));
    check(!git::affectsDiff(".git/objects/ab/cdef"));
    check(!git::affectsDiff(".git/logs/HEAD"));
};

auto tParseStatus = test("Git/parseStatus reads porcelain -z, renames included") = []
{
    auto entries = git::parseStatus(
        std::string {"M  staged.txt\0 M unstaged.txt\0MM both.txt\0?? new file.txt\0"
                     "R  new.txt\0old.txt\0",
                     74});

    check(entries.size() == 5);
    check(entries[0].path == "staged.txt" && entries[0].staged()
          && !entries[0].unstaged());
    check(!entries[1].staged() && entries[1].unstaged());
    check(entries[2].staged() && entries[2].unstaged());
    check(entries[3].path == "new file.txt" && entries[3].untracked());
    check(entries[4].path == "new.txt" && entries[4].index == 'R');
};

auto tBuilders = test("Git/index and remote commands") = []
{
    auto args = [](const eacp::Processes::ProcessOptions& options)
    {
        // Past the shared "-C repo -c ... -c ..." prefix.
        return std::vector<std::string> {options.arguments.begin() + 6,
                                         options.arguments.end()};
    };

    using Args = std::vector<std::string>;

    check(args(git::stage("r", {"a", "b c"}))
          == Args {"add", "-A", "--", "a", "b c"});
    check(args(git::unstage("r", {"a"}, true))
          == Args {"restore", "--staged", "--", "a"});
    check(args(git::unstage("r", {"a"}, false))
          == Args {"rm", "--cached", "-r", "-q", "--", "a"});
    check(args(git::unstageAll("r", true)) == Args {"reset", "-q"});
    check(args(git::commit("r", "Fix it\n\nBody", false))
          == Args {"commit", "-q", "-m", "Fix it\n\nBody"});
    check(args(git::commit("r", "m", true))
          == Args {"commit", "-q", "--amend", "-m", "m"});
    check(args(git::discard("r", {"x"}))
          == Args {"restore", "--source=HEAD", "--staged", "--worktree", "--", "x"});
    check(args(git::pull("r")) == Args {"pull", "--ff-only"});
    check(args(git::pushSettingUpstream("r", "origin", "main"))
          == Args {"push", "-u", "origin", "main"});
    check(git::push("r").mergeErrorOutput);
};

auto tStageCommit = test("Git/stage, unstage and commit in a real repo") = []
{
    auto repo = freshRepo("stage-commit");
    write(repo, "tracked.txt", "changed\n");
    write(repo, "new.txt", "hi\n");

    git::run(git::stage(repo, {"tracked.txt"}));
    auto entry = [&](const std::string& path)
    {
        for (const auto& item: git::parseStatus(*git::run(git::status(repo))))
            if (item.path == path)
                return item;

        return git::StatusEntry {};
    };

    check(entry("new.txt").untracked());
    check(entry("tracked.txt").staged() && !entry("tracked.txt").unstaged());

    git::run(git::unstage(repo, {"tracked.txt"}, true));
    check(!entry("tracked.txt").staged() && entry("tracked.txt").unstaged());

    git::run(git::stageAll(repo));
    check(git::run(git::commit(repo, "second\n\nwith a body", false)).has_value());
    check(trim(*git::run(git::lastCommitMessage(repo))) == "second\n\nwith a body");
    check(git::parseStatus(*git::run(git::status(repo))).empty());
};
