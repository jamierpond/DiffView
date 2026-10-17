#pragma once

#include <optional>
#include <string>

namespace diffview
{
enum class Action
{
    down,
    up,
    top,
    bottom,
    halfPageDown,
    halfPageUp,
    focusTree,
    focusDiff,
    togglePane,
    open,
    nextHunk,
    previousHunk,
    nextFile,
    previousFile,
    nextUnseen,
    previousUnseen,
    nextAnnotation,
    previousAnnotation,
    annotate,
    annotateHunk,
    visual,
    deleteAnnotation,
    toggleResolved,
    clearResolved,
    changeBase,
    refresh,
    search,
    searchBackward,
    searchNext,
    searchPrevious,
    commentFile,
    generalNote,
    toggleAllFiles,
    toggleTreeSort,
    submitRound,
    commandLine,
    openInEditor,
    openWithSystem,
    pickFiles,
    grepDiffs,
    pickComments,
    help,
    quit,
    cancel
};

struct Command
{
    Action action = Action::cancel;
    int count = 1;
};

// Keys are named "j", "G", " " (the leader), "C-d", "Tab", "Enter", "Esc".
class KeyParser
{
public:
    // nullopt while a sequence is pending, or when it meant nothing.
    std::optional<Command> feed(const std::string& key);

    std::string pending() const;

    void reset();

private:
    std::optional<Command> finish(Action action);

    int count = 0;
    std::string prefix;
};
} // namespace diffview
