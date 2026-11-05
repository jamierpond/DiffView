#include "Viewer.h"

#include <Text.h>

#include <ftxui/dom/elements.hpp>

#include <filesystem>

namespace diffview
{
bool Viewer::onTreeGitKey(const std::string& key)
{
    if (key == " ")
        toggleStageAtCursor();
    else if (key == "a")
        toggleStageAll();
    else if (key == "c")
        startCommit(false);
    else if (key == "A")
        startCommit(true);
    else if (key == "P")
        pushChanges();
    else if (key == "p")
        runGit("Pulling", git::pull(repo), true);
    else if (key == "d")
        askDiscard();
    else
        return false;

    return true;
}

void Viewer::toggleStageAtCursor()
{
    if (treeCursor >= static_cast<int>(treeRows.size()))
        return;

    const auto& row = treeRows[static_cast<std::size_t>(treeCursor)];
    auto under = row.path + "/";
    auto paths = std::vector<std::string> {};
    auto anyUnstaged = false;

    for (const auto& [path, entry]: stageStatus)
        if (path == row.path || (row.isDirectory && path.starts_with(under)))
        {
            paths.push_back(path);
            anyUnstaged = anyUnstaged || entry.unstaged();
        }

    if (paths.empty())
    {
        say("Nothing to stage here");
        return;
    }

    // Anything left unstaged: stage it all. All staged already: unstage.
    if (anyUnstaged)
        runGit("Staging", git::stage(repo, paths), false);
    else
        runGit("Unstaging", git::unstage(repo, paths, hasHead), false);
}

void Viewer::toggleStageAll()
{
    auto anyUnstaged =
        std::any_of(stageStatus.begin(),
                    stageStatus.end(),
                    [](const auto& item) { return item.second.unstaged(); });

    if (stageStatus.empty())
        say("Nothing to stage");
    else if (anyUnstaged)
        runGit("Staging all", git::stageAll(repo), false);
    else
        runGit("Unstaging all", git::unstageAll(repo, hasHead), false);
}

eacp::Threads::Async<void> Viewer::startCommit(bool amend)
{
    auto staged =
        std::count_if(stageStatus.begin(),
                      stageStatus.end(),
                      [](const auto& item) { return item.second.staged(); });

    if (!amend && staged == 0)
    {
        say("Nothing staged: space stages a file, a stages everything");
        co_return;
    }

    if (amend && !hasHead)
    {
        say("No commit to amend yet");
        co_return;
    }

    auto previous = std::string {};

    if (amend)
    {
        auto result =
            co_await eacp::Processes::runAsync(git::lastCommitMessage(repo));
        previous = trim(result.output);
    }

    editingId = 0;
    pendingAnnotation.reset();
    openEditor((amend ? "Amend the last commit · " : "Commit · ")
                   + std::to_string(staged) + " staged · :wq commits, empty aborts",
               previous);
    editorPurpose = amend ? EditorPurpose::amend : EditorPurpose::commit;
}

void Viewer::finishCommit(const std::string& message, bool amend)
{
    if (trim(message).empty())
    {
        say("Commit aborted: empty message");
        return;
    }

    runGit(
        amend ? "Amending" : "Committing", git::commit(repo, message, amend), false);
}

void Viewer::askDiscard()
{
    if (treeCursor >= static_cast<int>(treeRows.size()))
        return;

    const auto& row = treeRows[static_cast<std::size_t>(treeCursor)];
    auto entry = stageStatus.find(row.path);

    if (row.isDirectory || entry == stageStatus.end())
    {
        say("Discard works on one changed file");
        return;
    }

    discardPath = row.path;
    discardUntracked = entry->second.untracked();
    modal = Modal::confirmDiscard;
}

void Viewer::discardConfirmed()
{
    if (discardUntracked)
    {
        auto error = std::error_code {};
        std::filesystem::remove(std::filesystem::path {repo} / discardPath, error);
        say(error ? "Couldn't delete " + discardPath : "Deleted " + discardPath);
        refresh();
        return;
    }

    runGit("Discarding", git::discard(repo, {discardPath}), false);
}

eacp::Threads::Async<void> Viewer::pushChanges()
{
    busy = "Pushing";
    busySince = nowMs();
    auto upstream = co_await eacp::Processes::runAsync(git::upstream(repo));

    if (upstream.exitCode != 0)
    {
        auto branch = co_await eacp::Processes::runAsync(git::currentBranch(repo));
        busy.clear();
        pushBranch = trim(branch.output);

        if (pushBranch.empty() || pushBranch == "HEAD")
        {
            showOutput("Push", "Not on a branch: nothing to push.");
            co_return;
        }

        modal = Modal::confirmPushUpstream;
        redraw();
        co_return;
    }

    busy.clear();
    runGit("Pushing", git::push(repo), true);
}

eacp::Threads::Async<void> Viewer::pushSettingUpstream()
{
    co_await runGit(
        "Pushing", git::pushSettingUpstream(repo, "origin", pushBranch), true);
}

eacp::Threads::Async<void> Viewer::runGit(std::string label,
                                          eacp::Processes::ProcessOptions options,
                                          bool showResult)
{
    busy = label;
    busySince = nowMs();
    redraw();

    auto result = co_await eacp::Processes::runAsync(std::move(options));
    busy.clear();

    auto text = trim(result.output + result.errorOutput);

    if (!result.launched)
        showOutput(label + " failed", "Couldn't run git.");
    else if (result.exitCode != 0)
        showOutput(label + " failed",
                   text.empty() ? "git exited with an error" : text);
    else if (showResult)
        showOutput(label + " done", text.empty() ? "Done." : text);
    else
        say(label + " done");

    refresh();
}

void Viewer::showOutput(std::string title, std::string text)
{
    outputTitle = std::move(title);
    outputText = std::move(text);
    modal = Modal::output;
    redraw();
}

bool Viewer::onGitModalEvent(const std::string& key)
{
    auto yes = key == "y" || key == "Y";

    switch (modal)
    {
        case Modal::output:
            modal = Modal::none;
            break;

        case Modal::confirmDiscard:
            modal = Modal::none;

            if (yes)
                discardConfirmed();
            break;

        case Modal::confirmPushUpstream:
            modal = Modal::none;

            if (yes)
                pushSettingUpstream();
            break;

        default:
            break;
    }

    return true;
}

ftxui::Element Viewer::renderGitModal()
{
    using namespace ftxui;
    using ftxui::text;

    switch (modal)
    {
        case Modal::confirmDiscard:
            return window(
                ftxui::text(" Discard "),
                vbox({ftxui::text(
                          discardUntracked
                              ? "Delete " + discardPath
                                    + "? It is untracked: this removes the file."
                              : "Discard every change to " + discardPath
                                    + " (staged and unstaged)?"),
                      ftxui::text(""),
                      ftxui::text("y discard · n cancel") | bold}));

        case Modal::confirmPushUpstream:
            return window(
                ftxui::text(" Push "),
                vbox({ftxui::text(pushBranch + " has no upstream."),
                      ftxui::text("Push it with -u origin " + pushBranch + "?"),
                      ftxui::text(""),
                      ftxui::text("y push · n cancel") | bold}));

        case Modal::output:
        {
            auto lines = Elements {};

            for (const auto& line: splitLines(outputText))
            {
                lines.push_back(ftxui::text(displayable(line)));

                if (lines.size() > 30)
                    break;
            }

            lines.push_back(ftxui::text(""));
            lines.push_back(ftxui::text("any key closes") | dim);
            return window(ftxui::text(" " + outputTitle + " "),
                          vbox(std::move(lines)))
                   | size(WIDTH, LESS_THAN, 100);
        }

        default:
            break;
    }

    return ftxui::text("");
}
} // namespace diffview
