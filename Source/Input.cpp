#include "Viewer.h"

#include <Search.h>
#include <Text.h>
#include <TextBuffer.h>

#include <eacp/Core/Process/Process.h>

#include <algorithm>

namespace diffview
{
namespace
{
std::string keyName(const ftxui::Event& event)
{
    using ftxui::Event;

    static const auto named = std::vector<std::pair<Event, const char*>> {
        {Event::Return, "Enter"},     {Event::Escape, "Esc"},
        {Event::Tab, "Tab"},          {Event::Backspace, "Backspace"},
        {Event::Delete, "Delete"},    {Event::ArrowDown, "Down"},
        {Event::ArrowUp, "Up"},       {Event::ArrowLeft, "Left"},
        {Event::ArrowRight, "Right"}, {Event::PageDown, "PageDown"},
        {Event::PageUp, "PageUp"},    {Event::Home, "Home"},
        {Event::End, "End"},          {Event::F1, "F1"},
        {Event::CtrlA, "C-a"},        {Event::CtrlC, "C-c"},
        {Event::CtrlD, "C-d"},        {Event::CtrlE, "C-e"},
        {Event::CtrlJ, "C-j"},        {Event::CtrlK, "C-k"},
        {Event::CtrlN, "C-n"},        {Event::CtrlP, "C-p"},
        {Event::CtrlU, "C-u"},        {Event::CtrlW, "C-w"},
    };

    for (const auto& [match, name]: named)
        if (event == match)
            return name;

    if (event.is_character())
        return event.character();

    return {};
}

const std::vector<std::string>& commandNames()
{
    static const auto names = std::vector<std::string> {"base",
                                                        "comments",
                                                        "edit",
                                                        "files",
                                                        "grep",
                                                        "help",
                                                        "q",
                                                        "q!",
                                                        "quit",
                                                        "submit"};
    return names;
}

std::vector<std::string>
    previewLines(const FileDiff& file, int aroundFlat = -1, int radius = 30)
{
    auto out = std::vector<std::string> {};
    auto flat = 0;

    for (const auto& hunk: file.hunks)
    {
        if (aroundFlat < 0)
            out.push_back(hunk.header());

        for (const auto& line: hunk.lines)
        {
            if (aroundFlat < 0 || std::abs(flat - aroundFlat) <= radius)
                out.push_back((flat == aroundFlat ? "▶" : "") + line.prefixed());

            ++flat;
        }

        if (out.size() > 300)
            break;
    }

    if (file.binary)
        out.push_back("Binary file");

    return out;
}
} // namespace

bool Viewer::onEvent(const ftxui::Event& event)
{
    if (event == ftxui::Event::Custom)
        return false;

    if (editing)
        return onEditorEvent(event);

    auto key = keyName(event);

    if (key.empty())
        return true;

    switch (modal)
    {
        case Modal::help:
            modal = Modal::none;
            return true;
        case Modal::picker:
            return onPickerEvent(key);
        case Modal::confirmSubmit:
        case Modal::confirmQuit:
            return onConfirmEvent(key);
        case Modal::none:
            break;
    }

    if (prompt != Prompt::none)
        return onPromptEvent(key);

    if (key == "C-c")
    {
        keys.reset();
        requestQuit();
        return true;
    }

    if (auto command = keys.feed(key))
        run(*command);

    return true;
}

void Viewer::run(const Command& command)
{
    auto count = command.count;

    switch (command.action)
    {
        case Action::down:
            focus == Pane::tree ? moveTree(count) : moveCursor(count);
            break;
        case Action::up:
            focus == Pane::tree ? moveTree(-count) : moveCursor(-count);
            break;
        case Action::top:
            focus == Pane::tree ? moveTree(-1000000) : jumpToRow(0);
            break;
        case Action::bottom:
            focus == Pane::tree ? moveTree(1000000)
                                : jumpToRow(static_cast<int>(rows.size()) - 1);
            break;
        case Action::halfPageDown:
            focus == Pane::tree ? moveTree(count * std::max(1, treeHeight / 2))
                                : moveCursor(count * std::max(1, diffHeight / 2));
            break;
        case Action::halfPageUp:
            focus == Pane::tree ? moveTree(-count * std::max(1, treeHeight / 2))
                                : moveCursor(-count * std::max(1, diffHeight / 2));
            break;
        case Action::focusTree:
            focus = Pane::tree;
            break;
        case Action::focusDiff:
            // In the tree, l is Enter.
            if (focus == Pane::tree)
                run({Action::open, 1});
            else
                focus = Pane::diff;
            break;
        case Action::togglePane:
            focus = focus == Pane::tree ? Pane::diff : Pane::tree;
            break;
        case Action::open:
            if (focus == Pane::tree
                && treeCursor < static_cast<int>(treeRows.size()))
            {
                const auto& row = treeRows[static_cast<std::size_t>(treeCursor)];

                if (row.isDirectory)
                {
                    if (!collapsed.erase(row.path))
                        collapsed.insert(row.path);

                    rebuildTree();
                }
                else
                {
                    select(row.path);
                    focus = Pane::diff;
                }
            }
            else if (focus == Pane::diff)
            {
                if (annotation(annotationAtCursor()) != nullptr)
                    editAtCursor();
                else
                    annotateAtCursor(false);
            }
            break;
        case Action::nextHunk:
            stepHunk(1, count);
            break;
        case Action::previousHunk:
            stepHunk(-1, count);
            break;
        case Action::nextFile:
            stepFile(1, count);
            break;
        case Action::previousFile:
            stepFile(-1, count);
            break;
        case Action::nextUnseen:
            stepUnseen(1);
            break;
        case Action::previousUnseen:
            stepUnseen(-1);
            break;
        case Action::nextAnnotation:
            stepAnnotation(1, count);
            break;
        case Action::previousAnnotation:
            stepAnnotation(-1, count);
            break;
        case Action::annotate:
            annotateAtCursor(false);
            break;
        case Action::annotateHunk:
            annotateAtCursor(true);
            break;
        case Action::commentFile:
            commentOnFile();
            break;
        case Action::toggleAllFiles:
            allFiles = !allFiles;
            say(allFiles ? "All files" : "Changed files only");
            refresh();
            break;
        case Action::toggleTreeSort:
            flatBySize = !flatBySize;
            rebuildTree();
            break;
        case Action::generalNote:
            commentGeneral();
            break;
        case Action::visual:
            if (visualAnchor)
                visualAnchor.reset();
            else if (!rows.empty())
            {
                focus = Pane::diff;
                visualAnchor = diffCursor;
            }
            break;
        case Action::deleteAnnotation:
            deleteAtCursor();
            break;
        case Action::toggleResolved:
            toggleResolvedAtCursor();
            break;
        case Action::clearResolved:
            clearResolved();
            break;
        case Action::submitRound:
            if (draftCount(annotations) == 0)
                say("Nothing to submit: no new or changed comments");
            else
                modal = Modal::confirmSubmit;
            break;
        case Action::changeBase:
            openPicker(PickerKind::base);
            break;
        case Action::refresh:
            refresh();
            say("Refreshed");
            break;
        case Action::search:
            openPrompt(Prompt::search, true);
            break;
        case Action::searchBackward:
            openPrompt(Prompt::search, false);
            break;
        case Action::searchNext:
            searchAgain(true);
            break;
        case Action::searchPrevious:
            searchAgain(false);
            break;
        case Action::commandLine:
            openPrompt(Prompt::command, true);
            break;
        case Action::openInEditor:
            openInEditor();
            break;
        case Action::openWithSystem:
            openWithSystem();
            break;
        case Action::pickFiles:
            openPicker(PickerKind::files);
            break;
        case Action::grepDiffs:
            openPicker(PickerKind::grep);
            break;
        case Action::pickComments:
            openPicker(PickerKind::comments);
            break;
        case Action::help:
            modal = Modal::help;
            break;
        case Action::quit:
            requestQuit();
            break;
        case Action::cancel:
            visualAnchor.reset();
            break;
    }
}

void Viewer::openPrompt(Prompt kind, bool forward)
{
    prompt = kind;
    promptLine.set({});
    searchForward = forward;
    searchPane = focus;
    searchOriginCursor = focus == Pane::tree ? treeCursor : diffCursor;
    searchOriginTop = focus == Pane::tree ? treeTop : diffTop;
    liveHit.reset();
}

bool Viewer::onPromptEvent(const std::string& key)
{
    auto restoreOrigin = [&]
    {
        if (prompt != Prompt::search)
            return;

        if (searchPane == Pane::tree)
        {
            treeCursor = searchOriginCursor;
            treeTop = searchOriginTop;

            if (treeCursor < static_cast<int>(treeRows.size())
                && !treeRows[static_cast<std::size_t>(treeCursor)].isDirectory)
                select(treeRows[static_cast<std::size_t>(treeCursor)].path);
        }
        else
        {
            diffCursor = searchOriginCursor;
            diffTop = searchOriginTop;
        }
    };

    auto close = [&]
    {
        prompt = Prompt::none;
        liveHit.reset();
    };

    if (key == "Esc" || key == "C-c"
        || (key == "Backspace" && promptLine.text().empty()))
    {
        restoreOrigin();
        close();
        return true;
    }

    if (key == "Enter")
    {
        auto text = promptLine.text();

        if (prompt == Prompt::command)
        {
            close();
            runCommandLine(text);
            return true;
        }

        if (text.empty())
        {
            close();
            searchAgain(true);
            return true;
        }

        lastSearch = text;
        lastSearchForward = searchForward;

        if (!liveHit)
            say("Pattern not found: " + text);

        close();
        return true;
    }

    if (key == "Tab" && prompt == Prompt::command)
    {
        completeCommandLine();
        return true;
    }

    if (promptLine.feed(key) && prompt == Prompt::search)
        updateIncrementalSearch();

    return true;
}

std::optional<Viewer::SearchHit> Viewer::findFrom(int row,
                                                  bool forward,
                                                  const std::string& needle,
                                                  bool& wrapped) const
{
    // Rows as drawn, so match positions are drawing positions.
    auto lines = std::vector<std::string> {};

    if (searchPane == Pane::tree)
    {
        for (const auto& treeRow: treeRows)
            lines.push_back(treeRow.name);
    }
    else
    {
        const auto* file = selectedFile();

        for (const auto& diffRow: rows)
        {
            if (diffRow.kind == RowKind::line && file != nullptr)
                lines.push_back(
                    displayable(file->hunks[static_cast<std::size_t>(diffRow.hunk)]
                                    .lines[static_cast<std::size_t>(diffRow.line)]
                                    .text));
            else if (diffRow.kind == RowKind::hunkHeader && file != nullptr)
                lines.push_back(displayable(
                    file->hunks[static_cast<std::size_t>(diffRow.hunk)].header()));
            else
                lines.push_back(diffRow.text);
        }
    }

    if (lines.empty())
        return std::nullopt;

    row = std::clamp(row, 0, static_cast<int>(lines.size()) - 1);

    auto from = text::Position {
        row, forward ? lines[static_cast<std::size_t>(row)].size() : 0};
    auto result = text::search(lines, needle, from, forward);

    if (!result)
        return std::nullopt;

    wrapped = result->wrapped;
    return SearchHit {result->at.line, result->at.column, result->length};
}

void Viewer::updateIncrementalSearch()
{
    auto needle = promptLine.text();

    if (searchPane == Pane::tree)
        treeCursor = searchOriginCursor;
    else
        diffCursor = searchOriginCursor;

    liveHit.reset();

    if (needle.empty())
        return;

    auto wrapped = false;
    auto hit = findFrom(searchOriginCursor, searchForward, needle, wrapped);

    if (!hit)
        return;

    liveHit = hit;

    if (searchPane == Pane::tree)
    {
        treeCursor = hit->row;
        const auto& row = treeRows[static_cast<std::size_t>(hit->row)];

        if (!row.isDirectory)
            select(row.path);
    }
    else
    {
        diffCursor = hit->row;
    }
}

void Viewer::searchAgain(bool sameDirection)
{
    if (lastSearch.empty())
    {
        say("No previous search");
        return;
    }

    searchPane = focus;
    auto forward = sameDirection ? lastSearchForward : !lastSearchForward;
    auto cursor = focus == Pane::tree ? treeCursor : diffCursor;
    auto wrapped = false;
    auto hit = findFrom(cursor, forward, lastSearch, wrapped);

    if (!hit)
    {
        say("Pattern not found: " + lastSearch);
        return;
    }

    if (focus == Pane::tree)
    {
        treeCursor = hit->row;
        const auto& row = treeRows[static_cast<std::size_t>(hit->row)];

        if (!row.isDirectory)
            select(row.path);
    }
    else
    {
        diffCursor = hit->row;
        centerCursor(); // n -> nzzzv
    }

    if (wrapped)
        say(forward ? "search hit BOTTOM, continuing at TOP"
                    : "search hit TOP, continuing at BOTTOM");
    else
        say((forward ? "/" : "?") + lastSearch);
}

void Viewer::completeCommandLine()
{
    auto typed = promptLine.text();

    if (typed.find(' ') != std::string::npos)
        return;

    auto matches = std::vector<std::string> {};

    for (const auto& name: commandNames())
        if (name.starts_with(typed))
            matches.push_back(name);

    if (matches.empty())
        return;

    auto common = matches.front();

    for (const auto& name: matches)
        while (!name.starts_with(common))
            common.pop_back();

    if (matches.size() == 1 && (common == "base" || common == "grep"))
        common += " ";

    if (common.size() > typed.size())
        promptLine.set(common);
    else
    {
        auto list = std::string {};

        for (const auto& name: matches)
            list += name + "  ";

        say(list);
    }
}

void Viewer::runCommandLine(const std::string& line)
{
    auto text = trim(line);
    auto space = text.find(' ');
    auto name = text.substr(0, space);
    auto argument =
        space == std::string::npos ? std::string {} : trim(text.substr(space));

    if (name.empty())
        return;

    if (name == "submit")
        submitReview();
    else if (name == "q" || name == "quit")
        requestQuit();
    else if (name == "q!" || name == "quit!")
        screen.Exit();
    else if (name == "base")
    {
        if (argument.empty())
            openPicker(PickerKind::base);
        else
        {
            base = git::BaseSpec::parse(argument);
            say("Base: " + base.label());
            refresh();
        }
    }
    else if (name == "note")
        commentGeneral();
    else if (name == "resolved")
        clearResolved();
    else if (name == "comments")
        openPicker(PickerKind::comments);
    else if (name == "files")
        openPicker(PickerKind::files);
    else if (name == "grep")
    {
        openPicker(PickerKind::grep);
        pickerLine.set(argument);
        refreshPickerItems();
    }
    else if (name == "edit")
        openInEditor();
    else if (name == "help")
        modal = Modal::help;
    else
        say("Not a command: " + name);
}

void Viewer::openPicker(PickerKind kind)
{
    pickerKind = kind;
    pickerLine.set({});
    picker = telescope::Picker {};
    picker.setFuzzy(kind != PickerKind::grep);
    modal = Modal::picker;

    if (kind == PickerKind::base)
    {
        baseChoices = git::baseChoices({}, {});
        loadBaseChoices();
    }

    refreshPickerItems();
}

eacp::Threads::Async<void> Viewer::loadBaseChoices()
{
    auto branchOutput = co_await eacp::Processes::runAsync(git::branches(repo));
    auto logOutput =
        co_await eacp::Processes::runAsync(git::recentCommits(repo, 40));

    baseChoices = git::baseChoices(splitLines(branchOutput.output),
                                   git::parseCommits(logOutput.output));

    if (modal == Modal::picker && pickerKind == PickerKind::base)
        refreshPickerItems();

    redraw();
}

void Viewer::refreshPickerItems()
{
    auto items = std::vector<telescope::Item> {};

    switch (pickerKind)
    {
        case PickerKind::files:
            for (const auto& path: fileOrder())
            {
                const auto* file = changedFile(path);
                items.push_back({path,
                                 file == nullptr
                                     ? std::string {}
                                     : std::string(1, statusLetter(file->status))
                                           + " +" + std::to_string(file->added)
                                           + " -" + std::to_string(file->removed),
                                 path});
            }

            picker.preview = [this](const telescope::Item& item)
            {
                const auto* file = fileAt(item.key);
                return file != nullptr ? previewLines(*file)
                                       : std::vector<std::string> {};
            };
            break;

        case PickerKind::grep:
            for (const auto& hit: grepDiffs(files, pickerLine.text(), 500))
                items.push_back({hit.path + ":" + std::to_string(hit.lineNumber)
                                     + ": " + trim(hit.line.substr(1)),
                                 {},
                                 hit.path + "\n" + std::to_string(hit.flat)});

            picker.preview = [this](const telescope::Item& item)
            {
                auto newline = item.key.find('\n');
                const auto* file = fileAt(item.key.substr(0, newline));

                if (file == nullptr)
                    return std::vector<std::string> {};

                return previewLines(
                    *file, std::stoi(item.key.substr(newline + 1)), 12);
            };
            break;

        case PickerKind::comments:
        {
            auto sorted = std::vector<const Annotation*> {};

            for (const auto& note: annotations)
                sorted.push_back(&note);

            std::stable_sort(sorted.begin(),
                             sorted.end(),
                             [](const Annotation* a, const Annotation* b)
                             {
                                 auto rank = [](const Annotation* n)
                                 { return n->draft ? std::string {"~"} : n->round; };

                                 if (rank(a) != rank(b))
                                     return rank(a) > rank(b);

                                 return a->id > b->id;
                             });

            for (const auto* note: sorted)
            {
                auto first = splitLines(note->text);
                auto where = note->path.empty() ? std::string {"General"}
                             : note->fileLevel  ? note->path + " (file)"
                                               : note->path + ":" + lineLabel(*note);
                auto when = note->draft ? std::string {"draft"}
                                        : "sent " + roundLabel(note->round);

                if (note->withdrawn)
                    when += " · withdrawn";
                else if (note->resolved)
                    when += " · resolved";

                items.push_back({where + "  " + (first.empty() ? "" : first.front()),
                                 when,
                                 std::to_string(note->id)});
            }

            picker.preview = [this](const telescope::Item& item)
            {
                auto* note = annotation(std::stoi(item.key));

                if (note == nullptr)
                    return std::vector<std::string> {};

                auto out = std::vector<std::string> {};

                for (const auto* group: {&note->before, &note->lines})
                    out.insert(out.end(), group->begin(), group->end());

                out.insert(out.end(), note->after.begin(), note->after.end());
                out.push_back("");

                for (auto& line: splitLines(note->text))
                    out.push_back(line);

                return out;
            };
            break;
        }

        case PickerKind::base:
            for (const auto& choice: baseChoices)
                items.push_back({choice.value, choice.description, choice.value});

            picker.preview = [](const telescope::Item&)
            { return std::vector<std::string> {}; };
            break;
    }

    picker.setItems(std::move(items));

    if (pickerKind != PickerKind::grep)
        picker.setQuery(pickerLine.text());
}

bool Viewer::onPickerEvent(const std::string& key)
{
    if (key == "Esc" || key == "C-c")
        modal = Modal::none;
    else if (key == "Enter")
        acceptPicker();
    else if (key == "Down" || key == "C-n" || key == "C-j")
        picker.move(1);
    else if (key == "Up" || key == "C-p" || key == "C-k")
        picker.move(-1);
    else if (key == "PageDown" || key == "C-d")
        picker.move(10);
    else if (key == "PageUp" || key == "C-u")
        picker.move(-10);
    else if (pickerLine.feed(key))
    {
        if (pickerKind == PickerKind::grep)
            refreshPickerItems();
        else
            picker.setQuery(pickerLine.text());
    }

    return true;
}

void Viewer::acceptPicker()
{
    modal = Modal::none;
    const auto* item = picker.current();

    switch (pickerKind)
    {
        case PickerKind::files:
            if (item != nullptr)
            {
                select(item->key);
                focus = Pane::diff;
            }
            break;

        case PickerKind::grep:
            if (item != nullptr)
            {
                auto newline = item->key.find('\n');
                jumpToFlat(item->key.substr(0, newline),
                           std::stoi(item->key.substr(newline + 1)));
            }
            break;

        case PickerKind::comments:
            if (item != nullptr)
            {
                const auto* note = annotation(std::stoi(item->key));

                if (note != nullptr && !note->path.empty()
                    && fileAt(note->path) == nullptr)
                {
                    say(note->path + " isn't in the diff against " + base.label());
                    break;
                }

                jumpToAnnotation(std::stoi(item->key));
                editAtCursor();
            }
            break;

        case PickerKind::base:
        {
            auto value = item != nullptr ? item->key : trim(pickerLine.text());

            if (value.empty())
                return;

            base = git::BaseSpec::parse(value);
            say("Base: " + base.label());
            refresh();
            break;
        }
    }
}

bool Viewer::onConfirmEvent(const std::string& key)
{
    auto yes = key == "y" || key == "Y" || key == "Enter";
    auto no = key == "n" || key == "N";

    if (modal == Modal::confirmSubmit)
    {
        if (yes)
            submitReview();
        else if (no || key == "Esc" || key == "C-c")
            modal = Modal::none;

        return true;
    }

    if (yes)
    {
        submitReview();
        screen.Exit();
    }
    else if (no)
        screen.Exit();
    else if (key == "c" || key == "Esc" || key == "C-c")
        modal = Modal::none;

    return true;
}
} // namespace diffview
