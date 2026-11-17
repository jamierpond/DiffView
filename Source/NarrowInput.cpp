#include "Viewer.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace diffview
{
namespace
{
// Button presses and releases (1000), reported as SGR text (1006): what a
// phone terminal turns taps and one-finger drags into.
constexpr auto startMouseReports = "\x1b[?1000h\x1b[?1006h";
constexpr auto stopMouseReports = "\x1b[?1006l\x1b[?1000l";

// A phone terminal sends one wheel step per line the finger drags.
constexpr auto linesPerWheelStep = 1;

std::optional<std::string> findConfirmKey(TapAction action)
{
    static const auto keys = std::vector<std::pair<TapAction, std::string>> {
        {TapAction::confirmYes, "y"},
        {TapAction::confirmNo, "n"},
        {TapAction::confirmCancel, "Esc"},
    };

    for (const auto& [tapped, key]: keys)
        if (tapped == action)
            return key;

    return std::nullopt;
}

std::optional<vim::Outcome> findEditorOutcome(TapAction action)
{
    static const auto outcomes = std::vector<std::pair<TapAction, vim::Outcome>> {
        {TapAction::saveComment, vim::Outcome::saveAndClose},
        {TapAction::cancelComment, vim::Outcome::discard},
    };

    for (const auto& [tapped, outcome]: outcomes)
        if (tapped == action)
            return outcome;

    return std::nullopt;
}

// The buttons that do what a key already does, from the diff.
std::optional<Command> findTapCommand(TapAction action)
{
    static const auto commands = std::vector<std::pair<TapAction, Command>> {
        {TapAction::openAtSelection, {Action::open, 1}},
        {TapAction::submitRound, {Action::submitRound, 1}},
    };

    for (const auto& [tapped, command]: commands)
        if (tapped == action)
            return command;

    return std::nullopt;
}

bool isTap(const ftxui::Mouse& mouse)
{
    return mouse.button == ftxui::Mouse::Left
           && mouse.motion == ftxui::Mouse::Pressed;
}
} // namespace

bool Viewer::onEventInCurrentLayout(const ftxui::Event& event)
{
    if (event.is_mouse())
    {
        if (isNarrow)
            onNarrowMouseEvent(event);

        return true;
    }

    if (isNarrow && event != ftxui::Event::Custom)
        shouldRevealCursor = true;

    return onEvent(event);
}

void Viewer::onNarrowMouseEvent(ftxui::Event event)
{
    const auto& mouse = event.mouse();

    if (mouse.button == ftxui::Mouse::WheelUp)
    {
        scrollNarrowPage(-linesPerWheelStep);
        return;
    }

    if (mouse.button == ftxui::Mouse::WheelDown)
    {
        scrollNarrowPage(linesPerWheelStep);
        return;
    }

    if (!isTap(mouse))
        return;

    if (auto target = findTapTarget(tapTargets, {mouse.x, mouse.y}))
        runTapAction(*target);
}

void Viewer::runTapAction(const TapTarget& target)
{
    if (auto key = findConfirmKey(target.action))
    {
        onConfirmEvent(*key);
        return;
    }

    if (auto outcome = findEditorOutcome(target.action))
    {
        applyEditorOutcome(*outcome);
        return;
    }

    if (auto command = findTapCommand(target.action))
    {
        run(*command);
        return;
    }

    if (target.action == TapAction::goBack)
        goBackOnePage();
    else if (target.action == TapAction::openEntry)
        openTreeEntry(target.index);
    else if (target.action == TapAction::selectRow)
        selectDiffRow(target.index);
}

void Viewer::openTreeEntry(int index)
{
    treeCursor = index;
    shouldRevealCursor = true;
    run({Action::open, 1});
}

void Viewer::selectDiffRow(int index)
{
    focus = Pane::diff;
    jumpToRow(index);
}

void Viewer::goBackOnePage()
{
    switch (findCurrentNarrowPage())
    {
        case NarrowPage::confirm:
            onConfirmEvent("Esc");
            break;
        case NarrowPage::comment:
            applyEditorOutcome(vim::Outcome::discard);
            break;
        case NarrowPage::diff:
            visualAnchor.reset();
            focus = Pane::tree;
            shouldRevealCursor = true;
            break;
        case NarrowPage::files:
            break;
    }
}

void Viewer::scrollNarrowPage(int lines)
{
    switch (findCurrentNarrowPage())
    {
        case NarrowPage::files:
            narrowFilesScroll += lines;
            break;
        case NarrowPage::diff:
            narrowDiffScroll += lines;
            break;
        case NarrowPage::comment:
        case NarrowPage::confirm:
            break;
    }
}

// FTXUI only asks for mouse reports at start-up, for every layout; the wide
// layout leaves the mouse to the terminal so text can be selected.
void Viewer::setMouseReporting(bool shouldReport)
{
    if (shouldReport == isMouseReporting)
        return;

    isMouseReporting = shouldReport;
    std::cout << (shouldReport ? startMouseReports : stopMouseReports) << std::flush;
}
} // namespace diffview
