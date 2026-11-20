#include "NarrowParts.h"
#include "Viewer.h"

#include <IntraLine.h>
#include <NarrowPalette.h>
#include <Text.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>

namespace diffview
{
using namespace ftxui;
using ftxui::text; // not the TextBuffer namespace

namespace
{
constexpr auto headerHeight = minimumTappableHeight;
constexpr auto buttonHeight = roomyTappableHeight;
constexpr auto infoLineHeight = 1;
constexpr auto footerHeight = infoLineHeight + buttonHeight;

constexpr auto selectionMarkWidth = 1;
constexpr auto markerAndSpaceWidth = 2;
constexpr auto threadFrameWidth = 2;
constexpr auto minimumTextWidth = 1;
constexpr auto minimumEditorHeight = 5;
constexpr auto largestQuoteShareOfBody = 3;
constexpr auto messageShownMs = std::int64_t {5000};
constexpr auto decimalBase = 10;

constexpr auto cursorMark = "▌";
constexpr auto selectedMark = "┃";
constexpr auto continuationMark = "↳";

Color convertToColor(Rgb rgb)
{
    return Color::RGB(rgb.red, rgb.green, rgb.blue);
}

std::optional<ChangeColours> findChangeColours(LineKind kind)
{
    switch (kind)
    {
        case LineKind::added:
            return narrowAddedColours;
        case LineKind::removed:
            return narrowRemovedColours;
        case LineKind::context:
            break;
    }

    return std::nullopt;
}

std::string findMarker(LineKind kind)
{
    switch (kind)
    {
        case LineKind::added:
            return "+";
        case LineKind::removed:
            return "-";
        case LineKind::context:
            break;
    }

    return " ";
}

Color findStatusColor(FileStatus status)
{
    switch (status)
    {
        case FileStatus::added:
        case FileStatus::untracked:
            return convertToColor(narrowAddedColours.marker);
        case FileStatus::deleted:
            return convertToColor(narrowRemovedColours.marker);
        case FileStatus::modified:
        case FileStatus::unchanged:
            break;
    }

    return Color::Default;
}

int countDigits(int number)
{
    auto digits = 1;

    while (number >= decimalBase)
    {
        number /= decimalBase;
        ++digits;
    }

    return digits;
}

int findLargestLineNumber(const FileDiff& file)
{
    if (file.hunks.empty())
        return 0;

    const auto& last = file.hunks.back();
    return std::max(last.oldStart + last.oldCount, last.newStart + last.newCount);
}

int findLineNumberWidth(const FileDiff& file)
{
    return countDigits(findLargestLineNumber(file));
}

int findLineTextWidth(const FileDiff& file, int width)
{
    auto gutter =
        selectionMarkWidth + findLineNumberWidth(file) + markerAndSpaceWidth;
    return std::max(minimumTextWidth, width - gutter);
}

std::string
    formatNumberColumn(int visualLine, int number, int pieceCount, int digits)
{
    if (visualLine == 0)
        return padLeft(std::to_string(number), digits);

    auto blank = std::string(static_cast<std::size_t>(digits - 1), ' ');

    if (visualLine < pieceCount)
        return blank + continuationMark;

    return blank + " ";
}

std::optional<TextRange> sliceRange(std::optional<TextRange> range, ByteRange piece)
{
    if (!range)
        return std::nullopt;

    auto start = std::max(range->start, piece.start);
    auto end = std::min(range->end, piece.start + piece.length);

    if (end <= start)
        return std::nullopt;

    return TextRange {start - piece.start, end - piece.start};
}

std::string findMarkFor(bool isCursor, bool isSelected)
{
    if (isCursor)
        return cursorMark;

    if (isSelected)
        return selectedMark;

    return " ";
}

// The path's end, which names the file, when the whole path does not fit.
std::string fitPathToWidth(const std::string& path, int width)
{
    constexpr auto ellipsisWidth = 1;

    if (codepointCount(path) <= width || width <= ellipsisWidth)
        return path;

    return "…" + std::string {keepLastCodepoints(path, width - ellipsisWidth)};
}

bool isTappableNarrowRow(RowKind kind)
{
    return kind != RowKind::fileHeader && kind != RowKind::message;
}

Element renderHeader(const NarrowHeader& header,
                     int width,
                     std::vector<TapTarget>& targets)
{
    if (header.backLabel.empty())
        return vbox({text(" " + trim(header.title)) | bold,
                     text(" " + header.detail) | dim})
               | size(HEIGHT, EQUAL, headerHeight);

    targets.push_back({{0, 0, width, headerHeight}, TapAction::goBack});

    auto backButton = text(" ‹ " + header.backLabel + " ") | bold | inverted;
    auto title = fitPathToWidth(header.title, width - 1);

    return vbox({hbox({backButton, filler(), text(header.detail + " ")}),
                 text(" " + title) | bold})
           | size(HEIGHT, EQUAL, headerHeight);
}

Element renderButtons(const std::vector<NarrowButton>& buttons,
                      ScreenArea bar,
                      std::vector<TapTarget>& targets)
{
    auto areas = layOutButtons(static_cast<int>(buttons.size()), bar);
    auto parts = Elements {};
    auto column = bar.column;

    for (auto i = std::size_t {0}; i < buttons.size(); ++i)
    {
        const auto& area = areas[i];
        auto gap = area.column - column;

        if (gap > 0)
            parts.push_back(text(std::string(static_cast<std::size_t>(gap), ' ')));

        parts.push_back(text(buttons[i].label) | bold | center | border
                        | size(WIDTH, EQUAL, area.width)
                        | size(HEIGHT, EQUAL, area.height));
        targets.push_back({area, buttons[i].action});
        column = area.column + area.width;
    }

    return hbox(std::move(parts));
}

// Puts the lines of one item that fall in the view on screen; returns the
// screen rows they take, to tap.
std::optional<ScreenArea>
    appendVisibleLines(Elements& shown, Elements lines, LineSpan span, LineSpan view)
{
    auto visible = findVisiblePart(span, view);

    if (!visible)
        return std::nullopt;

    for (auto line = visible->first; line < visible->end(); ++line)
    {
        auto index = static_cast<std::size_t>(line - span.first);

        if (index < lines.size())
            shown.push_back(std::move(lines[index]));
        else
            shown.push_back(text(""));
    }

    return ScreenArea {
        0, headerHeight + visible->first - view.first, 0, visible->height};
}

Element renderPage(Element header, Element body, int bodyHeight, Element footer)
{
    return vbox({std::move(header),
                 std::move(body) | size(HEIGHT, EQUAL, bodyHeight),
                 std::move(footer)});
}

int findBodyHeight(int height)
{
    return std::max(1, height - headerHeight - footerHeight);
}

ScreenArea widenToBody(ScreenArea rowsOnScreen, ScreenArea body)
{
    rowsOnScreen.column = body.column;
    rowsOnScreen.width = body.width;
    return rowsOnScreen;
}

int findShownLineNumber(const DiffLine& line)
{
    if (line.kind == LineKind::removed)
        return line.oldLine;

    return line.newLine;
}

Element renderChangeCounts(const FileDiff& file, const std::string& lead)
{
    return hbox({text(lead),
                 text("+" + std::to_string(file.added))
                     | color(convertToColor(narrowAddedColours.marker)),
                 text(" -" + std::to_string(file.removed))
                     | color(convertToColor(narrowRemovedColours.marker))});
}

Element renderNarrowVisualLine(const NarrowLineLook& look, int visualLine)
{
    auto pieceCount = static_cast<int>(look.pieces.size());
    auto number =
        formatNumberColumn(visualLine, look.number, pieceCount, look.digits);
    auto markerColor = Color {Color::Default};
    auto changedBackground = std::optional<Color> {};

    if (look.colours)
    {
        markerColor = convertToColor(look.colours->marker);
        changedBackground = convertToColor(look.colours->changedBackground);
    }

    auto gutter = hbox({text(number) | dim,
                        text(look.marker) | bold | color(markerColor),
                        text(" ")});

    if (look.isEmphasised)
        gutter = gutter | bgcolor(cursorBackground(true));

    auto content = text("");

    if (visualLine < pieceCount)
    {
        auto piece = look.pieces[static_cast<std::size_t>(visualLine)];
        content = highlightText(look.shown.substr(piece.start, piece.length),
                                sliceTokens(look.tokens, piece),
                                sliceRange(look.changed, piece),
                                changedBackground,
                                std::nullopt);
    }

    auto drawn = hbox({text(look.mark) | bold, gutter, content, filler()});

    if (!look.colours)
        return drawn;

    return drawn | bgcolor(convertToColor(look.colours->rowBackground));
}

// Keeps the lines nearest the comment box, with a line saying how many are cut.
Elements keepLastQuotedLines(Elements quote, int limit)
{
    auto count = static_cast<int>(quote.size());

    if (count <= limit)
        return quote;

    if (limit <= 0)
        return {};

    auto hidden = count - limit + 1;
    quote.erase(quote.begin(), quote.begin() + hidden);
    quote.insert(quote.begin(),
                 text(" ⋯ " + std::to_string(hidden) + " lines above") | dim);
    return quote;
}
} // namespace

Element Viewer::renderInCurrentLayout()
{
    auto terminal = Terminal::Size();
    isNarrow = isNarrowLayout(options.layout, terminal.dimx);
    setMouseReporting(isNarrow);

    if (!isNarrow)
    {
        threadIndent = wideThreadIndent;
        return render();
    }

    threadIndent.clear();
    return renderNarrow({0, 0, terminal.dimx, terminal.dimy});
}

NarrowPage Viewer::findCurrentNarrowPage() const
{
    auto isConfirming = modal == Modal::confirmSubmit || modal == Modal::confirmQuit;

    return findNarrowPage({.isConfirming = isConfirming,
                           .isEditingComment = editing,
                           .isFileListFocused = focus == Pane::tree});
}

Element Viewer::renderNarrow(ScreenArea page)
{
    tapTargets.clear();
    auto wantedWrap = std::max(minimumTextWidth,
                               page.width - selectionMarkWidth - threadFrameWidth);

    if (wantedWrap != wrapWidth)
    {
        wrapWidth = wantedWrap;
        rebuildRows();
    }

    auto drawn = renderNarrowPage(page);
    shouldRevealCursor = false;

    if (modal == Modal::help || modal == Modal::picker)
        return dbox({drawn, renderModal() | clear_under | center});

    return drawn;
}

Element Viewer::renderNarrowPage(ScreenArea page)
{
    switch (findCurrentNarrowPage())
    {
        case NarrowPage::files:
            return renderNarrowFiles(page);
        case NarrowPage::comment:
            return renderNarrowComment(page);
        case NarrowPage::confirm:
            return renderNarrowConfirm(page);
        case NarrowPage::diff:
            break;
    }

    return renderNarrowDiff(page);
}

Element Viewer::renderNarrowFooter(const std::string& hint,
                                   const std::vector<NarrowButton>& buttons,
                                   ScreenArea page)
{
    auto bar = ScreenArea {0, page.height - buttonHeight, page.width, buttonHeight};
    return vbox(
        {renderNarrowInfoLine(hint), renderButtons(buttons, bar, tapTargets)});
}

std::string Viewer::describeSubmitButton() const
{
    return "Submit (" + std::to_string(draftCount(annotations)) + ")";
}

Element Viewer::renderNarrowFiles(ScreenArea page)
{
    auto bodyHeight = findBodyHeight(page.height);
    treeHeight = bodyHeight;

    auto detail =
        "base " + describeBase() + " · " + std::to_string(files.size()) + " changed";
    auto header =
        renderHeader({{}, describeFileListTitle(), detail}, page.width, tapTargets);
    auto footer =
        renderNarrowFooter("Tap a file to see its diff",
                           {{describeSubmitButton(), TapAction::submitRound}},
                           page);

    return renderPage(
        header,
        renderNarrowFileList({0, headerHeight, page.width, bodyHeight}),
        bodyHeight,
        footer);
}

Element Viewer::renderNarrowFileList(ScreenArea body)
{
    auto count = static_cast<int>(treeRows.size());
    auto entryHeight = chooseEntryHeight(count, body.height);
    auto spans = stackSpans(std::vector<int>(treeRows.size(), entryHeight));
    auto view = LineSpan {narrowFilesScroll, body.height};

    if (shouldRevealCursor && treeCursor < count)
        view.first =
            scrollToReveal(view, spans[static_cast<std::size_t>(treeCursor)]);

    view.first = clampScroll(view, getTotalHeight(spans));
    narrowFilesScroll = view.first;

    auto lines = Elements {};

    if (!loaded)
        lines.push_back(text(" loading…") | dim);
    else if (treeRows.empty())
        lines.push_back(text(" no changes vs " + describeBase()) | dim);

    for (auto i = 0; i < count; ++i)
    {
        auto span = spans[static_cast<std::size_t>(i)];

        if (!findVisiblePart(span, view))
            continue;

        auto entry = renderNarrowFileEntry(
            treeRows[static_cast<std::size_t>(i)], i, entryHeight);

        if (auto area = appendVisibleLines(lines, std::move(entry), span, view))
            tapTargets.push_back(
                {widenToBody(*area, body), TapAction::openEntry, i});
    }

    return vbox(std::move(lines));
}

std::vector<Element>
    Viewer::renderNarrowFileEntry(const TreeRow& row, int index, int entryHeight)
{
    auto isCursor = index == treeCursor;
    auto lead =
        NarrowEntryLead {text(findMarkFor(isCursor, false)) | bold,
                         std::string(static_cast<std::size_t>(row.depth * 2), ' ')};

    auto lines = [&]
    {
        if (row.isDirectory)
            return renderNarrowDirectoryLines(row, lead);

        if (row.path.empty())
            return renderNarrowGeneralLines(lead);

        return renderNarrowFileLines(row, lead);
    }();

    while (static_cast<int>(lines.size()) < entryHeight)
        lines.push_back(text(""));

    if (!isCursor)
        return lines;

    for (auto& line: lines)
        line = cursorLine(hbox({line, filler()}), true);

    return lines;
}

std::vector<Element> Viewer::renderNarrowDirectoryLines(const TreeRow& row,
                                                        const NarrowEntryLead& lead)
{
    auto arrow = row.expanded ? "▾ " : "▸ ";

    return {hbox({lead.mark,
                  text(lead.indent + arrow) | dim,
                  text(row.name + "/") | color(Color::Blue)})};
}

std::vector<Element> Viewer::renderNarrowGeneralLines(const NarrowEntryLead& lead)
{
    auto notes = std::to_string(countOpenGeneralNotes());

    return {
        hbox({lead.mark, text(lead.indent + "◆ General") | color(Color::Yellow)}),
        text(" " + lead.indent + "  " + notes + " open notes") | dim};
}

std::vector<Element> Viewer::renderNarrowFileLines(const TreeRow& row,
                                                   const NarrowEntryLead& lead)
{
    auto file = changedFile(row.path);
    auto status = file ? file->get().status : FileStatus::unchanged;
    auto first = Elements {lead.mark,
                           text(lead.indent),
                           text(std::string(1, statusLetter(status)) + " ")
                               | color(findStatusColor(status)) | bold,
                           text(row.name) | bold};

    if (tracker.isRecent(row.path, nowMs(), recentWindowMs))
        first.push_back(text(" ◆") | color(Color::Magenta));

    if (tracker.isUnseen(row.path))
        first.push_back(text(" ●") | color(Color::Cyan));

    if (auto comments = countOpenComments(row.path); comments > 0)
        first.push_back(text(" ✎" + std::to_string(comments))
                        | color(Color::Yellow));

    auto lines = Elements {hbox(std::move(first))};

    if (file && !file->get().binary)
        lines.push_back(renderChangeCounts(file->get(), " " + lead.indent + "  "));

    return lines;
}

Element Viewer::renderNarrowDiff(ScreenArea page)
{
    auto bodyHeight = findBodyHeight(page.height);
    diffHeight = bodyHeight;

    auto action = describeOpenAtSelection();
    auto footer =
        renderNarrowFooter("Tap a line, then " + action,
                           {{action, TapAction::openAtSelection},
                            {describeSubmitButton(), TapAction::submitRound}},
                           page);
    auto body = renderNarrowDiffBody({0, headerHeight, page.width, bodyHeight});

    return renderPage(
        renderHeader(describeNarrowDiffHeader(), page.width, tapTargets),
        body,
        bodyHeight,
        footer);
}

NarrowHeader Viewer::describeNarrowDiffHeader() const
{
    auto header = NarrowHeader {"Files", "General notes", ""};
    auto selected = selectedFile();

    if (!selected || selected->get().path.empty())
        return header;

    const auto& file = selected->get();
    header.title = file.path;
    header.detail = std::string(1, statusLetter(file.status)) + " +"
                    + std::to_string(file.added) + " -"
                    + std::to_string(file.removed);
    return header;
}

Element Viewer::renderNarrowDiffBody(ScreenArea body)
{
    auto heights = std::vector<int> {};

    for (const auto& row: rows)
        heights.push_back(countNarrowRowHeight(row, body.width));

    auto spans = stackSpans(heights);
    auto view = LineSpan {narrowDiffScroll, body.height};
    auto count = static_cast<int>(rows.size());

    if (shouldRevealCursor && diffCursor < count)
        view.first =
            scrollToReveal(view, spans[static_cast<std::size_t>(diffCursor)]);

    view.first = clampScroll(view, getTotalHeight(spans));
    narrowDiffScroll = view.first;

    auto lines = Elements {};

    for (auto i = 0; i < count; ++i)
    {
        auto span = spans[static_cast<std::size_t>(i)];
        const auto& row = rows[static_cast<std::size_t>(i)];

        if (!findVisiblePart(span, view))
            continue;

        auto area = appendVisibleLines(
            lines, renderNarrowDiffRow(row, i, body.width), span, view);

        if (area && isTappableNarrowRow(row.kind))
            tapTargets.push_back(
                {widenToBody(*area, body), TapAction::selectRow, i});
    }

    return vbox(std::move(lines));
}

int Viewer::countNarrowRowHeight(const DiffRow& row, int width) const
{
    if (row.kind == RowKind::fileHeader)
        return 0;

    if (row.kind != RowKind::line)
        return 1;

    const auto& file = selectedFile()->get();
    const auto& line = file.hunks[static_cast<std::size_t>(row.hunk)]
                           .lines[static_cast<std::size_t>(row.line)];
    auto pieces =
        findWrapRanges(displayable(line.text), findLineTextWidth(file, width));

    return findDiffLineHeight(static_cast<int>(pieces.size()));
}

std::vector<Element>
    Viewer::renderNarrowDiffRow(const DiffRow& row, int index, int width)
{
    auto emphasis = findRowEmphasis(row, index);
    auto mark = text(findMarkFor(emphasis == RowEmphasis::cursor,
                                 emphasis == RowEmphasis::selected))
                | bold;

    switch (row.kind)
    {
        case RowKind::fileHeader:
            return {};
        case RowKind::line:
            return renderNarrowLine(row, index, width);
        case RowKind::message:
            return {text(" " + row.text) | dim};
        case RowKind::hunkHeader:
            return {hbox({mark, renderNarrowHunkHeader(row)})};
        default:
            break;
    }

    auto* note = annotation(row.annotation);

    if (note == nullptr)
        return {text("")};

    auto element =
        hbox({mark, renderThreadRow(row, *note, index, text(row.text)), filler()});

    if (emphasis == RowEmphasis::cursor)
        element = cursorLine(element, true);

    return {element};
}

Element Viewer::renderNarrowHunkHeader(const DiffRow& row) const
{
    const auto& file = selectedFile()->get();

    if (file.status == FileStatus::unchanged)
        return text("unchanged file") | dim;

    auto header = file.hunks[static_cast<std::size_t>(row.hunk)].header();
    return text(displayable(header)) | color(Color::Cyan);
}

std::vector<Element>
    Viewer::renderNarrowLine(const DiffRow& row, int index, int width)
{
    const auto& file = selectedFile()->get();
    const auto& hunk = file.hunks[static_cast<std::size_t>(row.hunk)];
    auto lineIndex = static_cast<std::size_t>(row.line);
    const auto& line = hunk.lines[lineIndex];
    auto emphasis = findRowEmphasis(row, index);

    auto look = NarrowLineLook {.shown = displayable(line.text),
                                .tokens = findLineTokens(row),
                                .digits = findLineNumberWidth(file),
                                .number = findShownLineNumber(line)};
    look.pieces = findWrapRanges(look.shown, findLineTextWidth(file, width));
    look.mark = findMarkFor(emphasis == RowEmphasis::cursor,
                            emphasis == RowEmphasis::selected);
    look.isEmphasised = emphasis != RowEmphasis::none;

    if (file.status != FileStatus::unchanged)
    {
        look.colours = findChangeColours(line.kind);
        look.marker = findMarker(line.kind);
    }

    if (auto span = findChangedSpanOfLine(hunk, lineIndex))
        look.changed = TextRange {span->start, span->start + span->length};

    auto lines = Elements {};
    auto height = findDiffLineHeight(static_cast<int>(look.pieces.size()));

    for (auto visualLine = 0; visualLine < height; ++visualLine)
        lines.push_back(renderNarrowVisualLine(look, visualLine));

    return lines;
}

Element Viewer::renderNarrowComment(ScreenArea page)
{
    drawnTarget = editorTarget();

    auto bodyHeight = findBodyHeight(page.height);
    auto quoteLimit = std::min(bodyHeight / largestQuoteShareOfBody,
                               std::max(0, bodyHeight - minimumEditorHeight));
    auto body = keepLastQuotedLines(renderNarrowQuote(page.width), quoteLimit);
    auto editorBoxHeight = bodyHeight - static_cast<int>(body.size());
    body.push_back(renderEditor(page.width, editorBoxHeight));

    auto hint = std::string {"Save adds it to the draft; Submit sends it"};

    if (replyCommentId)
        hint = "Save sends this reply at once";

    auto footer = renderNarrowFooter(
        hint,
        {{"Cancel", TapAction::cancelComment}, {"Save", TapAction::saveComment}},
        page);

    return renderPage(
        renderHeader({"Cancel", editorTitle, {}}, page.width, tapTargets),
        vbox(std::move(body)),
        bodyHeight,
        footer);
}

std::vector<Element> Viewer::renderNarrowQuote(int width)
{
    auto quote = Elements {};

    for (auto i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const auto& row = rows[static_cast<std::size_t>(i)];

        if (!isQuotedInComment(row))
            continue;

        for (auto& line: renderNarrowDiffRow(row, i, width))
            quote.push_back(std::move(line));
    }

    return quote;
}

bool Viewer::isQuotedInComment(const DiffRow& row) const
{
    if (replyCommentId)
        return row.annotation == *replyCommentId;

    return drawnTarget && row.kind == RowKind::line && row.flat >= drawnTarget->first
           && row.flat <= drawnTarget->last;
}

Element Viewer::renderNarrowConfirm(ScreenArea page)
{
    auto drafts = std::to_string(draftCount(annotations));
    auto title = std::string {"Quit"};
    auto question = drafts + " comments are not submitted yet. Submit them first?";
    auto note = std::string {"Quit keeps them as the draft for next time."};
    auto buttons = std::vector<NarrowButton> {{"Cancel", TapAction::confirmCancel},
                                              {"Quit", TapAction::confirmNo},
                                              {"Submit", TapAction::confirmYes}};

    if (modal == Modal::confirmSubmit)
    {
        title = "Submit review";
        question = "Send " + drafts + " new or changed comments as a review round?";
        note = "Agents read it in .diffview/latest.md.";
        buttons = {{"Cancel", TapAction::confirmCancel},
                   {"Submit", TapAction::confirmYes}};
    }

    auto body = vbox({text(""),
                      hbox({text(" "), paragraph(question)}),
                      text(""),
                      hbox({text(" "), paragraph(note) | dim})});

    return renderPage(renderHeader({"Cancel", title, {}}, page.width, tapTargets),
                      body,
                      findBodyHeight(page.height),
                      renderNarrowFooter({}, buttons, page));
}

// The narrow layout's comment box is a phone's width: its keys must fit in it.
std::string Viewer::describeEditorKeys() const
{
    auto isInsert = editor.mode() == vim::Mode::insert;

    if (isNarrow && isInsert)
        return "-- INSERT --  Esc normal · ^S save+close";

    if (isNarrow)
        return ":w save · :x save+close · :q! discard";

    if (isInsert)
        return "-- INSERT --  Esc normal · Ctrl-S save+close · Ctrl-V paste";

    return ":w save · :wq / :x save+close · :q! discard · i insert";
}

Element Viewer::renderNarrowInfoLine(const std::string& hint)
{
    if (prompt != Prompt::none)
        return renderPrompt();

    if (auto pending = keys.pending(); !pending.empty())
        return text(" " + pending) | bold;

    if (!message.empty() && nowMs() - messageAt < messageShownMs)
        return text(" " + message);

    return text(" " + hint) | dim;
}

std::string Viewer::describeOpenAtSelection()
{
    if (isCursorOn(RowKind::collapsedThread))
        return "Open thread";

    if (isCursorOn(RowKind::earlierReplies))
        return "Show replies";

    if (const auto* note = annotation(annotationAtCursor()); note != nullptr)
    {
        if (note->replies.empty())
            return "Edit comment";

        return "Reply";
    }

    if (visualAnchor)
        return "Comment on lines";

    if (isCursorOn(RowKind::hunkHeader))
        return "Comment on hunk";

    return "Comment";
}
} // namespace diffview
