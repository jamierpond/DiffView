#include "Viewer.h"

#include <Search.h>
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
Color statusColor(FileStatus status)
{
    switch (status)
    {
        case FileStatus::added:
        case FileStatus::untracked:
            return Color::Green;
        case FileStatus::deleted:
            return Color::Red;
        case FileStatus::unchanged:
            return Color::Default;
        case FileStatus::modified:
            break;
    }

    return Color::Yellow;
}

// A dark background rather than inverted, so +/- colours stay readable.
const auto selectionBackground = Color::RGB(38, 58, 96);

Element cursorLine(Element line, bool focused)
{
    return line | bgcolor(focused ? Color::RGB(58, 62, 84) : Color::RGB(40, 42, 54));
}

std::string padLeft(const std::string& text, int width)
{
    auto length = static_cast<int>(text.size());
    return length >= width
               ? text
               : std::string(static_cast<std::size_t>(width - length), ' ') + text;
}

std::string lineNumber(int number)
{
    return padLeft(number > 0 ? std::to_string(number) : std::string {}, 5);
}

int scrollFor(int cursor, int top, int height, int count)
{
    if (height <= 0)
        return 0;

    auto margin = std::min(3, height / 4);

    if (cursor < top + margin)
        top = cursor - margin;

    if (cursor >= top + height - margin)
        top = cursor - height + margin + 1;

    return std::clamp(top, 0, std::max(0, count - height));
}

Element withMatch(const std::string& text, std::size_t start, std::size_t length)
{
    if (start >= text.size())
        return ftxui::text(text);

    length = std::min(length, text.size() - start);
    return hbox({ftxui::text(text.substr(0, start)),
                 ftxui::text(text.substr(start, length)) | bgcolor(Color::Yellow)
                     | color(Color::Black),
                 ftxui::text(text.substr(start + length))});
}

std::string modeName(vim::Mode mode)
{
    switch (mode)
    {
        case vim::Mode::insert:
            return " INSERT ";
        case vim::Mode::command:
            return " COMMAND ";
        case vim::Mode::search:
            return " SEARCH ";
        case vim::Mode::normal:
            break;
    }

    return " NORMAL ";
}

} // namespace

Element Viewer::render()
{
    auto terminal = Terminal::Size();
    auto treeWidth = std::clamp(terminal.dimx * 3 / 10, 20, 50);
    auto bodyHeight = std::max(1, terminal.dimy - 2); // pane title + status bar
    auto diffWidth = std::max(10, terminal.dimx - treeWidth - 1);

    auto wantedWrap = std::max(10, diffWidth - 13); // less the "          │ " gutter

    if (wantedWrap != wrapWidth)
    {
        wrapWidth = wantedWrap;
        rebuildRows();
    }

    treeHeight = bodyHeight;
    diffHeight = bodyHeight + 1; // no title row: the file header is a row

    auto main = vbox(
        {hbox({renderTree(treeWidth, bodyHeight) | size(WIDTH, EQUAL, treeWidth),
               separator(),
               renderDiff(diffWidth, diffHeight) | flex})
             | flex,
         prompt != Prompt::none ? renderPrompt() : renderStatus()});

    if (modal == Modal::none)
        return main;

    return dbox({main, renderModal() | clear_under | center});
}

Element Viewer::renderTree(int width, int height)
{
    treeTop =
        scrollFor(treeCursor, treeTop, height, static_cast<int>(treeRows.size()));
    auto now = nowMs();
    auto lines = Elements {};

    auto title = std::string {allFiles ? " All files · " : " Changes · "}
                 + std::to_string(allFiles ? paths().size() : files.size())
                 + (flatBySize ? " · by size" : "");
    lines.push_back(text(title) | bold | (focus == Pane::tree ? inverted : dim));

    if (!loaded)
        lines.push_back(text(" loading…") | dim);
    else if (treeRows.empty())
        lines.push_back(text(" no changes vs " + base.label()) | dim);

    auto end = std::min(static_cast<int>(treeRows.size()), treeTop + height);

    for (auto i = treeTop; i < end; ++i)
    {
        const auto& row = treeRows[static_cast<std::size_t>(i)];
        auto indent = std::string(static_cast<std::size_t>(row.depth * 2), ' ');
        auto parts = Elements {text(" " + indent)};

        if (row.isDirectory)
        {
            auto unseenInside = false;
            auto recentInside = false;

            if (!row.expanded)
                for (const auto& file: files)
                    if (file.path.starts_with(row.path + "/"))
                    {
                        unseenInside = unseenInside || tracker.isUnseen(file.path);
                        recentInside =
                            recentInside
                            || tracker.isRecent(file.path, now, recentWindowMs);
                    }

            parts.push_back(text(row.expanded ? "▾ " : "▸ ") | dim);
            parts.push_back(text(row.name + "/") | color(Color::Blue));

            if (recentInside)
                parts.push_back(text(" ◆") | color(Color::Magenta));
            if (unseenInside)
                parts.push_back(text(" ●") | color(Color::Cyan));
        }
        else
        {
            if (row.path.empty())
            {
                auto notes = std::count_if(
                    annotations.begin(),
                    annotations.end(),
                    [](const Annotation& a)
                    { return a.path.empty() && !a.withdrawn && !a.resolved; });
                parts.push_back(text("◆ General (" + std::to_string(notes) + ")")
                                | color(Color::Yellow));
                auto line = hbox(std::move(parts));

                if (i == treeCursor)
                    line = cursorLine(line, focus == Pane::tree);

                lines.push_back(line);
                continue;
            }

            const auto* file = changedFile(row.path);
            auto recent = tracker.isRecent(row.path, now, recentWindowMs);
            auto letter = file != nullptr ? statusLetter(file->status) : ' ';

            parts.push_back(text(std::string(1, letter) + " ")
                            | color(file != nullptr ? statusColor(file->status)
                                                    : Color::Default));

            auto name = text(row.name);

            if (recent)
                name = name | color(Color::Magenta) | bold;

            parts.push_back(name);

            if (recent)
                parts.push_back(text(" ◆") | color(Color::Magenta));

            if (tracker.isUnseen(row.path))
                parts.push_back(text(" ●") | color(Color::Cyan));

            auto comments =
                std::count_if(annotations.begin(),
                              annotations.end(),
                              [&](const Annotation& a)
                              { return a.path == row.path && !a.resolved; });

            if (comments > 0)
                parts.push_back(text(" ✎" + std::to_string(comments))
                                | color(Color::Yellow));

            parts.push_back(filler());

            if (file != nullptr && !file->binary)
            {
                parts.push_back(text(" +" + std::to_string(file->added))
                                | color(Color::Green) | dim);
                parts.push_back(text(" -" + std::to_string(file->removed) + " ")
                                | color(Color::Red) | dim);
            }
        }

        auto line = hbox(std::move(parts));

        if (i == treeCursor)
            line = cursorLine(line, focus == Pane::tree);

        lines.push_back(line | size(WIDTH, LESS_THAN, width + 1));
    }

    return vbox(std::move(lines)) | flex;
}

Element Viewer::renderDiff(int width, int height)
{
    auto lines = Elements {};

    if (selectedFile() == nullptr)
        return vbox(std::move(lines)) | flex;

    if (!editing)
    {
        diffTop =
            scrollFor(diffCursor, diffTop, height, static_cast<int>(rows.size()));
        auto end = std::min(static_cast<int>(rows.size()), diffTop + height);

        for (auto i = diffTop; i < end; ++i)
            lines.push_back(renderRow(rows[static_cast<std::size_t>(i)], i, width));

        return vbox(std::move(lines)) | flex;
    }

    auto count = static_cast<int>(rows.size());
    auto target = editorTarget();
    drawnTarget = target;
    auto boxAt = std::min(diffCursor + 1, count);

    auto pastComments = [&](int index)
    {
        while (index < count && rows[static_cast<std::size_t>(index)].annotation != 0
               && rows[static_cast<std::size_t>(index)].kind != RowKind::line)
            ++index;

        return index;
    };

    auto hiddenReply = -1;

    if (editingId != 0)
    {
        for (auto i = 0; i < count; ++i)
            if (rows[static_cast<std::size_t>(i)].annotation == editingId)
            {
                boxAt = i;
                break;
            }
    }
    else if (replyCommentId)
    {
        if (auto* note = annotation(*replyCommentId))
            if (auto* pending = pendingReply(*note))
                hiddenReply = static_cast<int>(pending - note->replies.data());

        for (auto i = 0; i < count; ++i)
            if (rows[static_cast<std::size_t>(i)].annotation == replyCommentId
                && rows[static_cast<std::size_t>(i)].kind
                       == RowKind::annotationBottom)
                boxAt = i;
    }
    else if (pendingAnnotation && pendingAnnotation->fileLevel)
    {
        boxAt = pastComments(1);
    }
    else if (target)
    {
        for (auto i = 0; i < count; ++i)
            if (rows[static_cast<std::size_t>(i)].kind == RowKind::line
                && rows[static_cast<std::size_t>(i)].flat == target->last)
                boxAt = i + 1;

        boxAt = pastComments(boxAt);
    }

    // Row indices, -1 for the box.
    auto items = std::vector<int> {};
    auto boxItem = 0;
    auto firstTargetItem = -1;

    for (auto i = 0; i <= count; ++i)
    {
        if (i == boxAt)
        {
            boxItem = static_cast<int>(items.size());
            items.push_back(-1);
        }

        if (i == count)
            break;

        const auto& row = rows[static_cast<std::size_t>(i)];

        if (editingId != 0 && row.annotation == editingId)
            continue;

        if (hiddenReply >= 0 && row.annotation == replyCommentId
            && row.reply == hiddenReply)
            continue;

        if (target && firstTargetItem < 0 && row.kind == RowKind::line
            && row.flat == target->first)
            firstTargetItem = static_cast<int>(items.size());

        items.push_back(i);
    }

    auto boxHeight = std::min(editorHeight(width), std::max(3, height - 1));

    auto top = 0;

    while (top < static_cast<int>(items.size())
           && items[static_cast<std::size_t>(top)] >= 0
           && items[static_cast<std::size_t>(top)] < diffTop)
        ++top;

    // Only on open: yanking back to the target on every key felt like losing
    // control.
    if (editorJustOpened)
    {
        if (firstTargetItem >= 0 && firstTargetItem < top)
            top = std::max(0, firstTargetItem - 1);

        if (boxItem < top)
            top = std::max(0, boxItem - 1);

        if (boxItem - top + boxHeight > height)
            top = std::max(0, boxItem - (height - boxHeight));

        editorJustOpened = false;
    }
    else if (boxItem - top + boxHeight > height && boxHeight <= height)
    {
        top = std::max(0, boxItem - (height - boxHeight));
    }

    if (items[static_cast<std::size_t>(top)] >= 0)
        diffTop = items[static_cast<std::size_t>(top)];

    auto used = 0;

    for (auto k = top; k < static_cast<int>(items.size()) && used < height; ++k)
    {
        auto index = items[static_cast<std::size_t>(k)];

        if (index < 0)
        {
            lines.push_back(renderEditor(width, std::min(boxHeight, height - used)));
            used += boxHeight;
            continue;
        }

        lines.push_back(
            renderRow(rows[static_cast<std::size_t>(index)], index, width));
        ++used;
    }

    return vbox(std::move(lines)) | flex;
}

std::optional<LineRange> Viewer::editorTarget() const
{
    const auto* file = selectedFile();

    if (file == nullptr)
        return std::nullopt;

    if (editingId != 0 || replyCommentId)
    {
        auto id = editingId != 0 ? editingId : replyCommentId.value_or(0);

        for (const auto& note: annotations)
            if (note.id == id && !note.stale)
                return findAnchor(note, *file);

        return std::nullopt;
    }

    return pendingAnnotation ? findAnchor(*pendingAnnotation, *file) : std::nullopt;
}

int Viewer::editorHeight(int width)
{
    editor.setWrapWidth(std::max(1, width - 2));
    auto textRows = static_cast<int>(editor.visualRows().size());
    return std::max(3, textRows) + 3; // borders and the mode/command line
}

Element Viewer::renderEditor(int width, int boxHeight)
{
    editor.setWrapWidth(std::max(1, width - 2));

    const auto& lines = editor.lines();
    auto visual = editor.visualRows();
    auto cursorRow = editor.cursorVisualRow();
    auto cursorColumn = editor.cursorVisualColumn();
    auto body = Elements {};

    for (auto r = 0; r < static_cast<int>(visual.size()); ++r)
    {
        const auto& piece = visual[static_cast<std::size_t>(r)];
        auto textOfRow = lines[static_cast<std::size_t>(piece.line)].substr(
            static_cast<std::size_t>(piece.start),
            static_cast<std::size_t>(piece.end - piece.start));
        textOfRow = displayable(textOfRow);

        if (auto match = editor.searchMatch(); match && match->row == piece.line
                                               && match->column >= piece.start
                                               && match->column < piece.end)
        {
            body.push_back(
                withMatch(textOfRow,
                          static_cast<std::size_t>(match->column - piece.start),
                          static_cast<std::size_t>(match->length))
                | ftxui::focus);
            continue;
        }

        if (r != cursorRow || editor.mode() == vim::Mode::command)
        {
            body.push_back(text(textOfRow));
            continue;
        }

        auto bytes = std::size_t {0};

        for (auto column = 0; column < cursorColumn && bytes < textOfRow.size();
             ++column)
        {
            ++bytes;

            while (bytes < textOfRow.size()
                   && (static_cast<unsigned char>(textOfRow[bytes]) & 0xC0) == 0x80)
                ++bytes;
        }

        auto next = bytes;

        if (next < textOfRow.size())
        {
            ++next;

            while (next < textOfRow.size()
                   && (static_cast<unsigned char>(textOfRow[next]) & 0xC0) == 0x80)
                ++next;
        }

        auto under = next > bytes ? textOfRow.substr(bytes, next - bytes) : " ";
        auto cursor = text(under) | inverted;

        if (editor.mode() == vim::Mode::insert)
            cursor = cursor | underlined;

        body.push_back(hbox({text(textOfRow.substr(0, bytes)),
                             cursor | ftxui::focus,
                             text(textOfRow.substr(next))}));
    }

    auto footer = Element {};

    if (editor.promptChar() != 0)
        footer =
            text(std::string(1, editor.promptChar()) + editor.commandLine() + "█");
    else if (!editor.message().empty())
        footer = text(editor.message()) | color(Color::Red);
    else
        footer =
            text(editor.mode() == vim::Mode::insert
                     ? "-- INSERT --  Esc normal · Ctrl-S save+close · Ctrl-V paste"
                     : ":w save · :wq / :x save+close · :q! discard · i insert")
            | dim;

    return window(text(" " + editorTitle + " ") | bold,
                  vbox({vbox(std::move(body)) | yframe | flex, footer}))
           | color(Color::Yellow) | size(WIDTH, EQUAL, width)
           | size(HEIGHT, EQUAL, boxHeight);
}

Element Viewer::renderRow(const DiffRow& row, int index, int width)
{
    const auto* file = selectedFile();
    auto element = Element {};

    auto inVisual = visualAnchor && index >= std::min(*visualAnchor, diffCursor)
                    && index <= std::max(*visualAnchor, diffCursor);

    // incsearch without hlsearch: only while typing.
    auto match = prompt == Prompt::search && searchPane == Pane::diff && liveHit
                         && liveHit->row == index
                     ? liveHit
                     : std::nullopt;

    auto body = [&](const std::string& shown)
    { return match ? withMatch(shown, match->start, match->length) : text(shown); };

    switch (row.kind)
    {
        case RowKind::fileHeader:
        {
            auto title = file->path + "  " + statusLetter(file->status);

            if (file->path.empty())
                title = "General notes";
            else if (file->status == FileStatus::unchanged)
                title = file->path + "  unchanged";
            else if (!file->binary)
                title += "  +" + std::to_string(file->added) + " -"
                         + std::to_string(file->removed);

            element = hbox({text(" "), body(title) | bold, filler()})
                      | bgcolor(Color::RGB(40, 42, 54));
            break;
        }

        case RowKind::message:
            element = text("  " + row.text) | dim;
            break;

        case RowKind::hunkHeader:
            if (file->status == FileStatus::unchanged)
            {
                element = text("  unchanged file") | dim;
                break;
            }

            element = body(displayable(
                          file->hunks[static_cast<std::size_t>(row.hunk)].header()))
                      | color(Color::Cyan);
            break;

        case RowKind::line:
        {
            const auto& line = file->hunks[static_cast<std::size_t>(row.hunk)]
                                   .lines[static_cast<std::size_t>(row.line)];
            auto marker = line.kind == LineKind::added     ? "+"
                          : line.kind == LineKind::removed ? "-"
                                                           : " ";
            auto content = hbox({text(marker), body(displayable(line.text))});

            // A whole unchanged file: one line number, no +/- column.
            if (file->status == FileStatus::unchanged)
            {
                element = hbox({text(lineNumber(line.newLine)) | dim,
                                text(" │ ") | dim,
                                body(displayable(line.text))});
                break;
            }

            if (line.kind == LineKind::added)
                content = content | color(Color::Green);
            else if (line.kind == LineKind::removed)
                content = content | color(Color::Red);

            element = hbox({text(lineNumber(line.oldLine)) | dim,
                            text(lineNumber(line.newLine)) | dim,
                            text(" │") | dim,
                            content});
            break;
        }

        case RowKind::annotationTop:
        case RowKind::annotationText:
        case RowKind::replyHeader:
        case RowKind::annotationBottom:
        {
            auto* note = annotation(row.annotation);
            auto accent = note == nullptr || note->resolved ? Color::GrayDark
                          : note->stale                     ? Color::Red
                                                            : Color::Yellow;

            if (row.kind == RowKind::annotationTop)
            {
                auto label =
                    std::string {"┌─ comment #"} + std::to_string(row.annotation);

                if (note != nullptr)
                {
                    label += " · "
                             + (note->path.empty() ? std::string {"general"}
                                : note->fileLevel  ? std::string {"whole file"}
                                                   : lineLabel(*note));
                    label += note->draft ? " · draft"
                                         : " · submitted "
                                               + roundLabel(note->round).substr(6);

                    if (auto owner = findOwner(*note); !owner.empty())
                        label += " · @" + owner;
                    if (note->resolved)
                        label += " · resolved";
                    if (note->stale)
                        label += " · stale: its lines left the diff";
                }

                element = text("          " + label + " ") | color(accent);
            }
            else if (row.kind == RowKind::replyHeader)
            {
                const auto& reply =
                    note->replies[static_cast<std::size_t>(row.reply)];
                auto byAgent = reply.author != reviewerName;
                auto label =
                    "↳ " + (reply.author.empty() ? std::string {"?"} : reply.author)
                    + (reply.time.size() > 5 ? " · " + reply.time.substr(5) : "")
                    + (reply.fresh ? " · draft" : "");
                auto shown = text(label) | bold;

                if (byAgent)
                    shown = shown | color(Color::Cyan);

                element = hbox({text("          │ ") | color(accent), shown});
            }
            else if (row.kind == RowKind::annotationText)
            {
                auto shown = body(row.text);
                auto byAgent =
                    row.reply >= 0 && note != nullptr
                    && note->replies[static_cast<std::size_t>(row.reply)].author
                           != reviewerName;

                if (byAgent)
                    shown = shown | color(Color::Cyan);

                if (note != nullptr && note->resolved)
                    shown = shown | dim;

                element =
                    hbox({text(row.reply >= 0 ? "          │   " : "          │ ")
                              | color(accent),
                          shown});
            }
            else
            {
                element = text("          └─") | color(accent);
            }
            break;
        }
    }

    element = element | size(WIDTH, LESS_THAN, width + 1);

    if (editing)
    {
        const auto& target = drawnTarget;
        auto inTarget = target && row.kind == RowKind::line
                        && row.flat >= target->first && row.flat <= target->last;

        return inTarget ? element | bgcolor(selectionBackground) : element;
    }

    if (index == diffCursor)
        return cursorLine(element, focus == Pane::diff);

    if (inVisual)
        return element | bgcolor(selectionBackground);

    return element;
}

Element Viewer::renderStatus()
{
    auto mode = editing               ? modeName(editor.mode())
                : visualAnchor        ? std::string {" VISUAL "}
                : focus == Pane::tree ? std::string {" TREE "}
                                      : std::string {" DIFF "};
    auto shortSha = baseSha.substr(0, 7);
    auto unseen = tracker.unseenCount();
    auto drafts = draftCount(annotations);

    auto parts = Elements {
        text(mode) | bold | inverted,
        text(" base ") | dim,
        text(base.label()) | bold,
        text(shortSha.empty() ? "" : " (" + shortSha + ")") | dim,
        text("  │ ") | dim,
        text(std::to_string(files.size()) + " files"),
    };

    if (unseen > 0)
        parts.push_back(text("  ● " + std::to_string(unseen) + " unseen")
                        | color(Color::Cyan));

    if (drafts > 0)
        parts.push_back(text("  ✎ " + std::to_string(drafts) + " draft")
                        | color(Color::Yellow));

    if (!rounds.empty())
        parts.push_back(text("  sent " + roundLabel(rounds.back())) | dim);

    if (!listening.empty())
    {
        auto names = std::string {};

        for (const auto& name: listening)
            names += " @" + name;

        parts.push_back(text("  ◉" + names) | color(Color::Cyan));
    }

    parts.push_back(filler());

    auto pending = keys.pending();

    if (editing)
        parts.push_back(text("Ctrl-S save · :wq · :q! ") | bold);
    else if (!pending.empty())
        parts.push_back(text(pending + " ") | bold);
    else if (!message.empty() && nowMs() - messageAt < 5000)
        parts.push_back(text(message + " "));
    else
        parts.push_back(text("F1 help ") | dim);

    return hbox(std::move(parts));
}

Element Viewer::renderPrompt()
{
    auto lead = prompt == Prompt::command ? ":" : searchForward ? "/" : "?";
    const auto& typed = promptLine.text();
    auto at = promptLine.cursor();

    return hbox({text(lead),
                 text(typed.substr(0, at)),
                 text(at < typed.size() ? typed.substr(at, 1) : " ") | inverted,
                 text(at < typed.size() ? typed.substr(at + 1) : ""),
                 filler()});
}

Element Viewer::renderModal()
{
    switch (modal)
    {
        case Modal::picker:
            return renderPicker();
        case Modal::confirmSubmit:
        case Modal::confirmQuit:
            return renderConfirm();
        case Modal::help:
            return renderHelp();
        case Modal::none:
            break;
    }

    return text("");
}

Element Viewer::renderPicker()
{
    auto terminal = Terminal::Size();
    auto width = std::max(40, terminal.dimx * 9 / 10);
    auto height = std::max(10, terminal.dimy * 8 / 10);
    auto listHeight = height - 4;
    auto withPreview = width >= 100;
    auto listWidth = withPreview ? width * 45 / 100 : width - 2;

    auto title = std::string {};

    switch (pickerKind)
    {
        case PickerKind::files:
            title = " Changed files ";
            break;
        case PickerKind::grep:
            title = " Grep the diff ";
            break;
        case PickerKind::comments:
            title = " Comments ";
            break;
        case PickerKind::base:
            title = " Diff base · now " + base.label() + " (name... = merge-base) ";
            break;
    }

    const auto& visible = picker.visible();
    auto selected = picker.selected();
    auto first =
        std::clamp(selected - listHeight / 2,
                   0,
                   std::max(0, static_cast<int>(visible.size()) - listHeight));
    auto list = Elements {};

    for (auto r = first;
         r < std::min(static_cast<int>(visible.size()), first + listHeight);
         ++r)
    {
        const auto& item = picker.items()[static_cast<std::size_t>(
            visible[static_cast<std::size_t>(r)])];
        auto marks = picker.highlights(r);
        auto label = Elements {};

        for (std::size_t i = 0; i < item.label.size();)
        {
            auto end = i + 1;

            while (end < item.label.size()
                   && (static_cast<unsigned char>(item.label[end]) & 0xC0) == 0x80)
                ++end;

            auto piece = text(displayable(item.label.substr(i, end - i)));

            if (std::find(marks.begin(), marks.end(), i) != marks.end())
                piece = piece | color(Color::Yellow) | bold;

            label.push_back(piece);
            i = end;
        }

        auto detailWidth = static_cast<int>(codepointCount(item.detail)) + 1;
        auto line =
            hbox({hbox(std::move(label))
                      | size(WIDTH, LESS_THAN, std::max(8, listWidth - detailWidth)),
                  filler(),
                  text(" " + item.detail) | dim});

        list.push_back(r == selected ? line | inverted : line);
    }

    if (visible.empty())
        list.push_back(
            text(pickerKind == PickerKind::base
                     ? " no match: Enter uses what you typed"
                 : pickerKind == PickerKind::grep && pickerLine.text().empty()
                     ? " type to search every changed line"
                     : " no matches")
            | dim);

    auto query = hbox({text("> "),
                       text(pickerLine.text()),
                       text(" ") | inverted,
                       filler(),
                       text(std::to_string(visible.size()) + "/"
                            + std::to_string(picker.items().size()) + " ")
                           | dim});

    auto left = vbox({query, separator(), vbox(std::move(list)) | flex})
                | size(WIDTH, EQUAL, listWidth);

    if (!withPreview)
        return window(text(title), left) | size(WIDTH, EQUAL, width)
               | size(HEIGHT, EQUAL, height);

    auto preview = Elements {};

    if (const auto* item = picker.current())
        for (const auto& line: picker.preview(*item))
        {
            auto shown = text(displayable(line));

            if (line.starts_with("▶"))
                shown = shown | inverted;
            else if (line.starts_with("+"))
                shown = shown | color(Color::Green);
            else if (line.starts_with("-"))
                shown = shown | color(Color::Red);
            else if (line.starts_with("@@"))
                shown = shown | color(Color::Cyan);

            preview.push_back(shown);

            if (static_cast<int>(preview.size()) >= height - 2)
                break;
        }

    return window(text(title),
                  hbox({left, separator(), vbox(std::move(preview)) | flex}))
           | size(WIDTH, EQUAL, width) | size(HEIGHT, EQUAL, height);
}

Element Viewer::renderConfirm()
{
    auto drafts = std::to_string(draftCount(annotations));

    if (modal == Modal::confirmSubmit)
        return window(
            text(" Submit review "),
            vbox({text("Send " + drafts
                       + " new or changed comments as a review round?"),
                  text("Writes .diffview/reviews/<time>.md and .diffview/latest.md")
                      | dim,
                  text(""),
                  text("y submit · n cancel") | bold}));

    return window(
        text(" Quit "),
        vbox({text(drafts + " comments not submitted yet. Submit them first?"),
              text("n keeps them as the draft for next time") | dim,
              text(""),
              text("y submit and quit · n quit · c cancel") | bold}));
}

Element Viewer::renderHelp()
{
    auto row = [](const std::string& keys, const std::string& what)
    { return hbox({text(keys) | bold | size(WIDTH, EQUAL, 18), text(what)}); };

    return window(
               text(" Keys "),
               vbox({
                   row("j k  gg G", "move, top / bottom (counts: 5j)"),
                   row("Ctrl-d Ctrl-u", "half page down / up"),
                   row("h l  Tab", "tree / diff / switch pane"),
                   row("Enter  l", "tree: toggle directory, open file"),
                   row("a  s", "all files / changed only, tree / by size"),
                   row("]c [c  ]f [f", "next / previous hunk, file"),
                   row("]u [u  ]a [a", "next / previous unseen file, comment"),
                   row("/ ?  n N", "search forward / back, next / previous"),
                   row("Space pf / ps", "find a file / grep the diff"),
                   row("Space pc", "all comments (Enter amends)"),
                   separator(),
                   row("Enter  c", "comment on line (edits one already there)"),
                   row("V … c", "comment on a range"),
                   row("gh  C", "comment on hunk / whole file"),
                   row("gc  :note", "general note (about no file)"),
                   row("dd  x", "delete (withdraw if sent) / resolve"),
                   row("R", "reply on the comment's thread (sent with S)"),
                   row("X  :resolved", "show / hide resolved comments"),
                   row("S  :submit", "submit the draft as a review round"),
                   separator(),
                   row("e  o", "edit in $EDITOR / open with the OS"),
                   row("b  :base", "change base (branch... = merge-base)"),
                   row("r  :q  q", "refresh, quit"),
                   separator(),
                   text("Comment box: vim. i a o insert, Esc normal, :w :wq :q!,")
                       | dim,
                   text(
                       "/ ? n N search, Ctrl-S save+close, Ctrl-V paste (images too).")
                       | dim,
                   text("Tab after @ completes an agent's name.") | dim,
                   text("● unseen  ◆ touched in the last 10s  ✎ comments  ◉ agents "
                        "listening")
                       | dim,
                   text("Agents read .diffview/latest.md") | dim,
               }))
           | size(WIDTH, EQUAL, 64);
}
} // namespace diffview
