#pragma once

#include <eacp/Core/Process/Process.h>

#include <optional>
#include <string>
#include <vector>

namespace diffview::git
{
// The base of a repo with no commits yet.
constexpr auto emptyTree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

// "main..." means the merge-base of HEAD and main, as in `git diff main...`.
struct BaseSpec
{
    std::string ref = "HEAD";
    bool mergeBase = false;

    static BaseSpec parse(const std::string& text);
    std::string label() const;

    bool operator==(const BaseSpec&) const = default;
};

// GIT_OPTIONAL_LOCKS=0: a refresh must never take index.lock out from under
// an agent running git.
eacp::Processes::ProcessOptions command(const std::string& repo,
                                        std::vector<std::string> args);

eacp::Processes::ProcessOptions resolveBase(const std::string& repo,
                                            const BaseSpec& spec);

eacp::Processes::ProcessOptions diff(const std::string& repo,
                                     const std::string& baseSha);

eacp::Processes::ProcessOptions untracked(const std::string& repo);
eacp::Processes::ProcessOptions tracked(const std::string& repo);

eacp::Processes::ProcessOptions branches(const std::string& repo);

eacp::Processes::ProcessOptions recentCommits(const std::string& repo, int count);

struct Commit
{
    std::string sha;
    std::string subject;
};

std::vector<Commit> parseCommits(const std::string& output);

struct BaseChoice
{
    std::string value;
    std::string description;
};

std::vector<BaseChoice> baseChoices(const std::vector<std::string>& branchNames,
                                    const std::vector<Commit>& commits);

std::optional<std::string> run(eacp::Processes::ProcessOptions options);

std::optional<std::string> findRoot(const std::string& path);

// Inside .git only HEAD, the index and refs count: objects, logs and lock
// files churn on every git command, so each refresh would trigger another.
bool affectsDiff(const std::string& relativePath);
} // namespace diffview::git
