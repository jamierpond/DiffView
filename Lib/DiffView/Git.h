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

// --- working the index and the remote (the tree pane's lazygit keys) -------

// One path's two status columns, as `git status --porcelain` has them:
// `index` the staged change, `worktree` the unstaged one ('?' both for
// untracked).
struct StatusEntry
{
    std::string path;
    char index = ' ';
    char worktree = ' ';

    bool staged() const { return index != ' ' && index != '?'; }
    bool unstaged() const { return worktree != ' '; }
    bool untracked() const { return index == '?'; }

    bool operator==(const StatusEntry&) const = default;
};

eacp::Processes::ProcessOptions status(const std::string& repo);
std::vector<StatusEntry> parseStatus(const std::string& output);

eacp::Processes::ProcessOptions stage(const std::string& repo,
                                      const std::vector<std::string>& paths);

// Without a first commit there is no HEAD to restore the index from, so
// the paths are dropped from the index instead.
eacp::Processes::ProcessOptions unstage(const std::string& repo,
                                        const std::vector<std::string>& paths,
                                        bool hasHead);

eacp::Processes::ProcessOptions stageAll(const std::string& repo);
eacp::Processes::ProcessOptions unstageAll(const std::string& repo, bool hasHead);

eacp::Processes::ProcessOptions
    commit(const std::string& repo, const std::string& message, bool amend);

eacp::Processes::ProcessOptions lastCommitMessage(const std::string& repo);

// Both the staged and unstaged changes to tracked paths, back to HEAD.
// Untracked files aren't git's to discard: the caller deletes them.
eacp::Processes::ProcessOptions discard(const std::string& repo,
                                        const std::vector<std::string>& paths);

eacp::Processes::ProcessOptions currentBranch(const std::string& repo);
eacp::Processes::ProcessOptions upstream(const std::string& repo);
eacp::Processes::ProcessOptions push(const std::string& repo);
eacp::Processes::ProcessOptions pushSettingUpstream(const std::string& repo,
                                                    const std::string& remote,
                                                    const std::string& branch);
eacp::Processes::ProcessOptions pull(const std::string& repo);

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
