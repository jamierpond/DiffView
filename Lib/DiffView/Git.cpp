#include "Git.h"
#include "Text.h"

namespace diffview::git
{
BaseSpec BaseSpec::parse(const std::string& text)
{
    auto spec = BaseSpec {};
    auto ref = trim(text);

    if (ref.size() > 3 && ref.ends_with("..."))
    {
        spec.mergeBase = true;
        ref.resize(ref.size() - 3);
    }

    spec.ref = ref.empty() ? "HEAD" : ref;
    return spec;
}

std::string BaseSpec::label() const
{
    return mergeBase ? ref + "..." : ref;
}

eacp::Processes::ProcessOptions command(const std::string& repo,
                                        std::vector<std::string> args)
{
    auto options = eacp::Processes::ProcessOptions {};
    options.executable = "git";
    options.arguments = {
        "-C", repo, "-c", "core.quotePath=false", "-c", "color.ui=false"};

    for (auto& arg: args)
        options.arguments.push_back(std::move(arg));

    options.environment = {
        {"GIT_OPTIONAL_LOCKS", "0"}, {"GIT_PAGER", "cat"}, {"LC_ALL", "C"}};
    return options;
}

eacp::Processes::ProcessOptions resolveBase(const std::string& repo,
                                            const BaseSpec& spec)
{
    if (spec.mergeBase)
        return command(repo, {"merge-base", "HEAD", spec.ref});

    return command(repo,
                   {"rev-parse", "--verify", "--quiet", spec.ref + "^{commit}"});
}

eacp::Processes::ProcessOptions
    diff(const std::string& repo, const std::string& baseSha, int contextLines)
{
    return command(repo,
                   {"diff",
                    "--no-color",
                    "--no-ext-diff",
                    "--no-renames",
                    "--no-relative",
                    "--src-prefix=a/",
                    "--dst-prefix=b/",
                    "-U" + std::to_string(contextLines),
                    baseSha,
                    "--"});
}

eacp::Processes::ProcessOptions untracked(const std::string& repo)
{
    return command(repo, {"ls-files", "--others", "--exclude-standard", "-z"});
}

eacp::Processes::ProcessOptions tracked(const std::string& repo)
{
    return command(repo, {"ls-files", "-z"});
}

eacp::Processes::ProcessOptions branches(const std::string& repo)
{
    return command(repo,
                   {"for-each-ref",
                    "--sort=-committerdate",
                    "--format=%(refname:short)",
                    "refs/heads",
                    "refs/remotes"});
}

eacp::Processes::ProcessOptions recentCommits(const std::string& repo, int count)
{
    return command(repo, {"log", "-n", std::to_string(count), "--format=%h%x09%s"});
}

std::vector<Commit> parseCommits(const std::string& output)
{
    auto commits = std::vector<Commit> {};

    for (const auto& line: splitLines(output))
    {
        auto tab = line.find('\t');

        if (tab == std::string::npos)
            continue;

        commits.push_back({line.substr(0, tab), line.substr(tab + 1)});
    }

    return commits;
}

std::vector<BaseChoice> baseChoices(const std::vector<std::string>& branchNames,
                                    const std::vector<Commit>& commits)
{
    auto choices = std::vector<BaseChoice> {{"HEAD", "last commit (default)"}};

    for (const auto& name: branchNames)
    {
        // origin/HEAD is a symref alias of another remote branch.
        if (name.empty() || name.ends_with("/HEAD") || name == "origin")
            continue;

        choices.push_back({name + "...", "where HEAD forked from " + name});
        choices.push_back({name, "tip of " + name});
    }

    for (const auto& commit: commits)
        choices.push_back({commit.sha, commit.subject});

    return choices;
}

std::optional<std::string> run(eacp::Processes::ProcessOptions options)
{
    auto result = eacp::Processes::run(std::move(options));

    if (!result.launched || result.exitCode != 0)
        return std::nullopt;

    return result.output;
}

std::optional<std::string> findRoot(const std::string& path)
{
    auto root = git::run(command(path, {"rev-parse", "--show-toplevel"}));

    if (!root)
        return std::nullopt;

    auto trimmed = trim(*root);

    if (trimmed.empty())
        return std::nullopt;

    return trimmed;
}

bool affectsDiff(const std::string& relativePath)
{
    if (relativePath != ".git" && !relativePath.starts_with(".git/"))
        return true;

    if (relativePath.ends_with(".lock"))
        return false;

    return relativePath == ".git/HEAD" || relativePath == ".git/index"
           || relativePath == ".git/packed-refs"
           || relativePath.starts_with(".git/refs/");
}
} // namespace diffview::git
