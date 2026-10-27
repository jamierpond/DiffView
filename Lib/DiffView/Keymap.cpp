#include "Keymap.h"

#include <map>

namespace diffview
{
namespace
{
const std::map<std::string, Action>& singleKeys()
{
    static const auto keys = std::map<std::string, Action> {
        {"j", Action::down},
        {"Down", Action::down},
        {"k", Action::up},
        {"Up", Action::up},
        {"G", Action::bottom},
        {"End", Action::bottom},
        {"Home", Action::top},
        {"C-d", Action::halfPageDown},
        {"PageDown", Action::halfPageDown},
        {"C-u", Action::halfPageUp},
        {"PageUp", Action::halfPageUp},
        {"h", Action::focusTree},
        {"Left", Action::focusTree},
        {"l", Action::focusDiff},
        {"Right", Action::focusDiff},
        {"Tab", Action::togglePane},
        {"C-n", Action::nextFile},
        {"C-p", Action::previousFile},
        {"Enter", Action::open},
        {"n", Action::searchNext},
        {"N", Action::searchPrevious},
        {"a", Action::toggleAllFiles},
        {"f", Action::toggleFullContext},
        {"F", Action::clearFilter},
        {"s", Action::toggleTreeSort},
        {"c", Action::annotate},
        {"C", Action::commentFile},
        {"S", Action::submitRound},
        {":", Action::commandLine},
        {"V", Action::visual},
        {"e", Action::openInEditor},
        {"o", Action::openWithSystem},
        {"x", Action::toggleResolved},
        {"X", Action::clearResolved},
        {"R", Action::reply},
        {"b", Action::changeBase},
        {"r", Action::refresh},
        {"/", Action::search},
        {"?", Action::searchBackward},
        {"F1", Action::help},
        {"q", Action::quit},
        {"Esc", Action::cancel},
    };

    return keys;
}

const std::map<std::string, Action>& sequences()
{
    static const auto keys = std::map<std::string, Action> {
        {"gg", Action::top},
        {"dd", Action::deleteAnnotation},
        {"]c", Action::nextHunk},
        {"[c", Action::previousHunk},
        {"]f", Action::nextFile},
        {"[f", Action::previousFile},
        {"]a", Action::nextAnnotation},
        {"[a", Action::previousAnnotation},
        {"]u", Action::nextUnseen},
        {"[u", Action::previousUnseen},
        {"g?", Action::help},
        {"gc", Action::generalNote},
        {"gh", Action::annotateHunk},
        {"za", Action::toggleFold},
        {"zc", Action::closeFold},
        {"zo", Action::openFold},
        {"zM", Action::closeAllFolds},
        {"zR", Action::openAllFolds},
        {" h", Action::help},
        {" pf", Action::pickFiles},
        {" ps", Action::grepDiffs},
        {" pc", Action::pickComments},
    };

    return keys;
}
} // namespace

std::optional<Command> KeyParser::feed(const std::string& key)
{
    if (key == "Esc")
    {
        reset();
        return Command {Action::cancel, 1};
    }

    auto typed = prefix + key;
    auto startsSequence = false;

    for (const auto& [sequence, action]: sequences())
    {
        if (sequence == typed)
            return finish(action);

        if (sequence.size() > typed.size() && sequence.starts_with(typed))
            startsSequence = true;
    }

    if (startsSequence)
    {
        prefix = typed;
        return std::nullopt;
    }

    if (!prefix.empty())
    {
        reset();
        return std::nullopt;
    }

    if (key.size() == 1 && key[0] >= '0' && key[0] <= '9'
        && (key[0] != '0' || count > 0))
    {
        count = count * 10 + (key[0] - '0');

        if (count > 99999)
            count = 99999;

        return std::nullopt;
    }

    auto single = singleKeys().find(key);

    if (single == singleKeys().end())
    {
        reset();
        return std::nullopt;
    }

    return finish(single->second);
}

std::optional<Command> KeyParser::finish(Action action)
{
    auto command = Command {action, count > 0 ? count : 1};
    reset();
    return command;
}

std::string KeyParser::pending() const
{
    auto shown = prefix;

    if (shown.starts_with(" "))
        shown = "<leader>" + shown.substr(1);

    return (count > 0 ? std::to_string(count) : std::string {}) + shown;
}

void KeyParser::reset()
{
    count = 0;
    prefix.clear();
}
} // namespace diffview
