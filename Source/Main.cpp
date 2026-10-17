#include "Viewer.h"

#include <eacp/Core/App/App.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace
{
constexpr auto usage =
    "usage: diffview [--base <ref>] [<ref>|<branch>...] [repo]\n"
    "\n"
    "Shows the working tree's diff against a base (default HEAD), live,\n"
    "and sends your comments as review rounds to <repo>/.diffview/latest.md.\n";
} // namespace

int main(int argc, char* argv[])
{
    auto options = diffview::Options {};
    auto path = std::filesystem::current_path().string();

    for (auto i = 1; i < argc; ++i)
    {
        auto arg = std::string {argv[i]};

        if (arg == "-h" || arg == "--help")
        {
            std::fputs(usage, stdout);
            return 0;
        }

        if (arg == "--base" && i + 1 < argc)
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

    if (isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
    {
        std::fputs("diffview: needs a terminal\n", stderr);
        return 1;
    }

    options.repo = *root;
    return eacp::Apps::run<diffview::Viewer>(options);
}
