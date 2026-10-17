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
