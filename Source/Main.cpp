#include "Monitor.h"
#include "Viewer.h"

#include <eacp/Core/App/App.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace
{
constexpr auto usage =
    "usage: dv [--base <ref>] [<ref>|<branch>...] [repo]\n"
    "       dv monitor [--once] [repo]\n"
    "\n"
    "Shows the working tree's diff against a base (default HEAD), live,\n"
    "and sends your comments as review rounds to <repo>/.diffview/latest.md.\n"
    "\n"
    "  dv              this repo, against HEAD (working tree + index + untracked)\n"
    "  dv main...      against where HEAD forked from main\n"
    "  dv HEAD~3       against that exact commit\n"
    "  dv ~/other      another repo\n"
    "\n"
    "It is a terminal UI for a human. F1 inside it lists the keys.\n"
    "\n"
    "For coding agents: you don't run dv. The human watches your edits in it and\n"
    "comments line by line. When they say they've reviewed (or mention review\n"
    "comments), read <repo>/.diffview/latest.md:\n"
    "  - each `### path:lines · status · #id` heading is one comment; lines are\n"
    "    working-tree line numbers, `(old)` the base's, `(whole file)` the file\n"
    "  - the `>` lines in the block under it are the code it is about\n"
    "  - act on `open` comments; ignore `resolved` and `withdrawn`\n"
    "  - when you've dealt with one, change its `· open` to `· resolved` in that\n"
    "    file; dv picks it up\n"
    "Past rounds are in .diffview/reviews/.\n"
    "\n"
    "To hear when they submit, run `dv monitor` under your Monitor tool once\n"
    "you start work in a repo. It prints one line per submitted round, e.g.\n"
    "  dv: review round 2026-10-17-142300 submitted · 3 open · read <path>\n"
    "Read that path each time. --once exits after the first round.\n";
} // namespace

int main(int argc, char* argv[])
{
    auto options = diffview::Options {};
    auto path = std::filesystem::current_path().string();
    auto monitor = argc > 1 && std::string {argv[1]} == "monitor";
    auto once = false;

    for (auto i = monitor ? 2 : 1; i < argc; ++i)
    {
        auto arg = std::string {argv[i]};

        if (arg == "-h" || arg == "--help")
        {
            std::fputs(usage, stdout);
            return 0;
        }

        if (monitor)
        {
            if (arg == "--once")
                once = true;
            else if (!arg.starts_with("-") && std::filesystem::is_directory(arg))
                path = arg;
            else
            {
                std::fputs(usage, stderr);
                return 2;
            }
        }
        else if (arg == "--base" && i + 1 < argc)
            options.base = diffview::git::BaseSpec::parse(argv[++i]);
        // Like `git diff main`: a positional that isn't a directory is the base.
        else if (!arg.starts_with("-") && std::filesystem::is_directory(arg))
            path = arg;
        else if (!arg.starts_with("-"))
            options.base = diffview::git::BaseSpec::parse(arg);
        else
        {
            std::fputs(usage, stderr);
            return 2;
        }
    }

    auto root = diffview::git::findRoot(path);

    if (!root)
    {
        std::fprintf(
            stderr, "diffview: %s is not inside a git work tree\n", path.c_str());
        return 1;
    }

    if (monitor)
        return diffview::runMonitor(*root, once);

    if (isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
    {
        std::fputs("diffview: needs a terminal\n", stderr);
        return 1;
    }

    options.repo = *root;
    return eacp::Apps::run<diffview::Viewer>(options);
}
