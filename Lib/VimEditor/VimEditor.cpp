#include "VimEditor.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace vim
{
namespace
{
bool isContinuation(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

int size(const std::string& text)
{
    return static_cast<int>(text.size());
}

int nextChar(const std::string& line, int at)
{
    if (at >= size(line))
        return size(line);

    ++at;

    while (at < size(line) && isContinuation(line[static_cast<std::size_t>(at)]))
        ++at;

    return at;
}

int previousChar(const std::string& line, int at)
{
    if (at <= 0)
        return 0;

    --at;

    while (at > 0 && isContinuation(line[static_cast<std::size_t>(at)]))
        --at;

    return at;
}

int lastCharStart(const std::string& line)
{
    return line.empty() ? 0 : previousChar(line, size(line));
}

int codepoints(const std::string& line, int from, int to)
{
    auto count = 0;

    for (auto i = from; i < to; ++i)
        if (!isContinuation(line[static_cast<std::size_t>(i)]))
            ++count;

    return count;
}

int advance(const std::string& line, int from, int count, int limit)
{
    while (count > 0 && from < limit)
    {
        from = nextChar(line, from);
        --count;
    }

    return std::min(from, limit);
}

enum class CharClass
{
    space,
    word,
    punctuation
};

CharClass classOf(char c)
{
    auto byte = static_cast<unsigned char>(c);

    if (byte >= 0x80 || std::isalnum(byte) != 0 || c == '_')
        return CharClass::word;

    if (std::isspace(byte) != 0)
        return CharClass::space;

    return CharClass::punctuation;
}

int firstNonBlank(const std::string& line)
{
    auto at = 0;

    while (at < size(line)
           && (line[static_cast<std::size_t>(at)] == ' '
               || line[static_cast<std::size_t>(at)] == '\t'))
        ++at;

    return at >= size(line) ? lastCharStart(line) : at;
}

bool isPrintable(const std::string& key)
{
    if (key.empty())
        return false;

    auto first = static_cast<unsigned char>(key[0]);

    if (first >= 0x80)
        return true;

    return key.size() == 1 && first >= 0x20 && first != 0x7F;
}

std::vector<std::string> splitText(std::string_view text)
{
    auto lines = std::vector<std::string> {std::string {}};

    for (auto c: text)
    {
        if (c == '\n')
            lines.emplace_back();
        else if (c != '\r')
            lines.back() += c;
    }

    return lines;
}
} // namespace

struct Position
{
    int row = 0;
    int column = 0;

    bool operator==(const Position&) const = default;

    bool operator<(const Position& other) const
    {
        return row < other.row || (row == other.row && column < other.column);
    }
};

// Editor internals that work on positions, kept out of the header.
struct EditorOps
{
    Editor& e;

    const std::string& line(int row) { return e.buffer.line(row); }
    int rowCount() { return e.buffer.lineCount(); }

    static ::text::Position toText(Position p)
    {
        return {p.row, static_cast<std::size_t>(p.column)};
    }
    Position cursor() { return {e.cursorRow, e.cursorColumn}; }

    void setCursor(Position p)
    {
        e.cursorRow = std::clamp(p.row, 0, rowCount() - 1);
        e.cursorColumn = std::clamp(p.column, 0, size(line(e.cursorRow)));
    }

    void clampNormal()
    {
        e.cursorRow = std::clamp(e.cursorRow, 0, rowCount() - 1);
        const auto& current = line(e.cursorRow);

        if (e.currentMode == Mode::insert)
        {
            e.cursorColumn = std::clamp(e.cursorColumn, 0, size(current));
            return;
        }

        if (e.cursorColumn >= size(current))
            e.cursorColumn = lastCharStart(current);
    }

    void pushUndo() { e.buffer.snapshot(toText(cursor())); }

    // Class at a position; past the end of a line reads as whitespace.
    CharClass classAt(Position p)
    {
        const auto& text = line(p.row);

        if (p.column >= size(text))
            return CharClass::space;

        return classOf(text[static_cast<std::size_t>(p.column)]);
    }

    bool atLineEnd(Position p) { return p.column >= size(line(p.row)); }

    bool forward(Position& p)
    {
        if (!atLineEnd(p))
        {
            p.column = nextChar(line(p.row), p.column);

            if (!atLineEnd(p) || line(p.row).empty())
                return true;
        }

        if (p.row + 1 >= rowCount())
            return false;

        ++p.row;
        p.column = 0;
        return true;
    }

    bool backward(Position& p)
    {
        if (p.column > 0)
        {
            p.column = previousChar(line(p.row), p.column);
            return true;
        }

        if (p.row == 0)
            return false;

        --p.row;
        p.column = lastCharStart(line(p.row));
        return true;
    }

    Position wordForward(Position p)
    {
        auto start = classAt(p);

        if (start != CharClass::space)
            while (!atLineEnd(p) && classAt(p) == start)
                p.column = nextChar(line(p.row), p.column);

        while (true)
        {
            if (atLineEnd(p))
            {
                if (p.row + 1 >= rowCount())
                    return p;

                ++p.row;
                p.column = 0;

                if (line(p.row).empty())
                    return p;

                continue;
            }

            if (classAt(p) != CharClass::space)
                return p;

            p.column = nextChar(line(p.row), p.column);
        }
    }

    Position wordEnd(Position p)
    {
        if (!forward(p))
            return p;

        while (classAt(p) == CharClass::space)
            if (!forward(p))
                return p;

        auto cls = classAt(p);

        while (true)
        {
            auto next = nextChar(line(p.row), p.column);

            if (next >= size(line(p.row))
                || classOf(line(p.row)[static_cast<std::size_t>(next)]) != cls)
                return p;

            p.column = next;
        }
    }

    Position wordBack(Position p)
    {
        if (!backward(p))
            return p;

        while (classAt(p) == CharClass::space && !line(p.row).empty())
            if (!backward(p))
                return p;

        if (line(p.row).empty())
            return p;

        auto cls = classAt(p);

        while (p.column > 0)
        {
            auto before = previousChar(line(p.row), p.column);

            if (classOf(line(p.row)[static_cast<std::size_t>(before)]) != cls)
                break;

            p.column = before;
        }

        return p;
    }

    std::string extract(Position from, Position to)
    {
        if (from.row == to.row)
            return line(from.row).substr(
                static_cast<std::size_t>(from.column),
                static_cast<std::size_t>(to.column - from.column));

        auto text = line(from.row).substr(static_cast<std::size_t>(from.column));

        for (auto r = from.row + 1; r < to.row; ++r)
            text += "\n" + line(r);

        text += "\n" + line(to.row).substr(0, static_cast<std::size_t>(to.column));
        return text;
    }

    void erase(Position from, Position to, bool keepYank = false)
    {
        if (!(from < to))
            return;

        if (!keepYank)
        {
            e.yank = extract(from, to);
            e.yankLinewise = false;
        }

        e.buffer.erase(toText(from), toText(to));
        setCursor(from);
    }

    void insertRaw(std::string_view text)
    {
        auto end = e.buffer.insert(toText(cursor()), text);
        e.cursorRow = end.line;
        e.cursorColumn = static_cast<int>(end.column);
    }

    void deleteLines(int count, bool yankIt)
    {
        auto first = e.cursorRow;
        auto last = std::min(rowCount(), first + count);

        if (yankIt)
        {
            e.yank.clear();

            for (auto r = first; r < last; ++r)
                e.yank += (r == first ? "" : "\n") + line(r);

            e.yankLinewise = true;
        }

        e.buffer.eraseLines(first, last - first);

        e.cursorRow = std::min(first, rowCount() - 1);
        e.cursorColumn = firstNonBlank(line(e.cursorRow));
    }

    void yankLines(int count)
    {
        auto last = std::min(rowCount(), e.cursorRow + count);
        e.yank.clear();

        for (auto r = e.cursorRow; r < last; ++r)
            e.yank += (r == e.cursorRow ? "" : "\n") + line(r);

        e.yankLinewise = true;
    }

    void put(bool after, int count)
    {
        if (e.yank.empty() && !e.yankLinewise)
            return;

        pushUndo();

        if (e.yankLinewise)
        {
            auto pieces = splitText(e.yank);
            auto at = after ? e.cursorRow + 1 : e.cursorRow;

            for (auto n = 0; n < count; ++n)
                e.buffer.insertLines(at, pieces);

            e.cursorRow = at;
            e.cursorColumn = firstNonBlank(line(at));
            return;
        }

        if (after && !line(e.cursorRow).empty())
            e.cursorColumn = nextChar(line(e.cursorRow), e.cursorColumn);

        auto text = std::string {};

        for (auto n = 0; n < count; ++n)
            text += e.yank;

        insertRaw(text);
        e.cursorColumn = previousChar(line(e.cursorRow), e.cursorColumn);
    }

    std::pair<int, int> innerWord()
    {
        const auto& text = line(e.cursorRow);

        if (text.empty())
            return {0, 0};

        auto cls = classOf(text[static_cast<std::size_t>(e.cursorColumn)]);
        auto start = e.cursorColumn;
        auto end = nextChar(text, e.cursorColumn);

        while (start > 0)
        {
            auto before = previousChar(text, start);

            if (classOf(text[static_cast<std::size_t>(before)]) != cls)
                break;

            start = before;
        }

        while (end < size(text)
               && classOf(text[static_cast<std::size_t>(end)]) == cls)
            end = nextChar(text, end);

        return {start, end};
    }

    std::pair<int, int> aroundWord()
    {
        auto [start, end] = innerWord();
        const auto& text = line(e.cursorRow);
        auto trailing = end;

        while (trailing < size(text)
               && classOf(text[static_cast<std::size_t>(trailing)])
                      == CharClass::space)
            ++trailing;

        if (trailing > end)
            return {start, trailing};

        while (start > 0
               && classOf(text[static_cast<std::size_t>(start - 1)])
                      == CharClass::space)
            --start;

        return {start, end};
    }

    void join(int count)
    {
        for (auto n = 0; n < std::max(1, count - 1); ++n)
        {
            if (e.cursorRow + 1 >= rowCount())
                return;

            auto next = line(e.cursorRow + 1);
            auto at = 0;

            while (at < size(next)
                   && (next[static_cast<std::size_t>(at)] == ' '
                       || next[static_cast<std::size_t>(at)] == '\t'))
                ++at;

            next = next.substr(static_cast<std::size_t>(at));
            auto current = line(e.cursorRow);

            while (!current.empty() && current.back() == ' ')
                current.pop_back();

            auto joinAt = size(current);

            if (!current.empty() && !next.empty())
                current += " ";

            current += next;
            e.buffer.replaceLine(e.cursorRow, current);
            e.buffer.eraseLines(e.cursorRow + 1, 1);
            e.cursorColumn = joinAt;
        }
    }

    bool jumpToMatch(const std::optional<::text::SearchResult>& found,
                     const std::string& pattern,
                     bool forward)
    {
        if (!found)
        {
            e.notice = "E486: Pattern not found: " + pattern;
            return false;
        }

        if (found->wrapped)
            e.notice = forward ? "search hit BOTTOM, continuing at TOP"
                               : "search hit TOP, continuing at BOTTOM";

        e.cursorRow = found->at.line;
        e.cursorColumn = static_cast<int>(found->at.column);
        return true;
    }

    void moveVertical(int delta)
    {
        auto rows = e.visualRows();
        auto current = e.cursorVisualRow();

        if (e.desiredColumn < 0)
            e.desiredColumn = e.cursorVisualColumn();

        auto target =
            std::clamp(current + delta, 0, static_cast<int>(rows.size()) - 1);
        const auto& row = rows[static_cast<std::size_t>(target)];
        const auto& text = line(row.line);
        auto column = advance(text, row.start, e.desiredColumn, row.end);

        // Landing on a soft break would put the cursor on the next row.
        auto lastOfLine = row.end >= size(text);

        if (column >= row.end && row.end > row.start
            && (!lastOfLine || e.currentMode != Mode::insert))
            column = previousChar(text, row.end);

        e.cursorRow = row.line;
        e.cursorColumn = column;
    }
};

Editor::Editor(std::string text)
{
    setText(std::move(text));
}

void Editor::setText(std::string text)
{
    buffer.setText(text);
    cursorRow = 0;
    cursorColumn = 0;
    currentMode = Mode::normal;
    command.clear();
    pending.clear();
    desiredColumn = -1;
    savedText = this->text();
}

std::string Editor::text() const
{
    return buffer.text();
}

void Editor::insertText(std::string_view text)
{
    auto ops = EditorOps {*this};
    ops.pushUndo();

    if (currentMode == Mode::normal && !buffer.line(cursorRow).empty())
        cursorColumn = nextChar(buffer.line(cursorRow), cursorColumn);

    ops.insertRaw(text);
    ops.clampNormal();
    desiredColumn = -1;
}

bool Editor::modified() const
{
    return text() != savedText;
}

void Editor::markSaved()
{
    savedText = text();
}

Mode Editor::mode() const
{
    return currentMode;
}

const std::vector<std::string>& Editor::lines() const
{
    return buffer.lines();
}

int Editor::row() const
{
    return cursorRow;
}

int Editor::column() const
{
    return cursorColumn;
}

std::string Editor::commandLine() const
{
    return command;
}

char Editor::promptChar() const
{
    if (currentMode == Mode::command)
        return ':';

    if (currentMode == Mode::search)
        return searchForward ? '/' : '?';

    return 0;
}

std::optional<Editor::SearchMatch> Editor::searchMatch() const
{
    return currentMode == Mode::search ? liveMatch : std::nullopt;
}

std::string Editor::message() const
{
    return notice;
}

void Editor::setWrapWidth(int columns)
{
    wrapWidth = std::max(0, columns);
}

std::vector<Editor::VisualRow> Editor::visualRows() const
{
    auto rows = std::vector<VisualRow> {};

    const auto& all = buffer.lines();

    for (std::size_t l = 0; l < all.size(); ++l)
    {
        const auto& text = all[l];
        auto lineIndex = static_cast<int>(l);
        auto start = 0;

        if (wrapWidth > 0)
        {
            while (codepoints(text, start, size(text)) > wrapWidth)
            {
                auto cut = advance(text, start, wrapWidth, size(text));
                auto space = text.substr(0, static_cast<std::size_t>(cut))
                                 .rfind(' ', static_cast<std::size_t>(cut - 1));
                auto end = space != std::string::npos
                                   && static_cast<int>(space) >= start
                                   && static_cast<int>(space) + 1 > start
                               ? static_cast<int>(space) + 1
                               : cut;
                rows.push_back({lineIndex, start, end});
                start = end;
            }
        }

        rows.push_back({lineIndex, start, size(text)});
    }

    return rows;
}

int Editor::cursorVisualRow() const
{
    auto rows = visualRows();

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto& row = rows[i];

        if (row.line != cursorRow)
            continue;

        auto lastOfLine = i + 1 == rows.size() || rows[i + 1].line != cursorRow;

        if (cursorColumn < row.end || lastOfLine)
            return static_cast<int>(i);
    }

    return 0;
}

int Editor::cursorVisualColumn() const
{
    auto rows = visualRows();
    const auto& row = rows[static_cast<std::size_t>(cursorVisualRow())];
    return codepoints(buffer.line(cursorRow), row.start, cursorColumn);
}

Outcome Editor::feed(const std::string& key)
{
    notice.clear();
    auto ops = EditorOps {*this};
    auto keepDesired = false;

    if (key == "C-s")
    {
        currentMode = Mode::normal;
        command.clear();
        pending.clear();
        ops.clampNormal();
        markSaved();
        return Outcome::saveAndClose;
    }

    if (key == "C-v")
        return Outcome::paste;

    if (currentMode == Mode::search)
    {
        auto restore = [&]
        {
            cursorRow = searchStartRow;
            cursorColumn = searchStartColumn;
            currentMode = Mode::normal;
            command.clear();
            liveMatch.reset();
        };

        auto find = [&](const std::string& pattern, bool forward)
        {
            return ::text::search(
                buffer.lines(),
                pattern,
                {searchStartRow, static_cast<std::size_t>(searchStartColumn)},
                forward);
        };

        if (key == "Esc" || key == "C-c")
            restore();
        else if (key == "Enter")
        {
            auto pattern = command.empty() ? lastPattern : command;
            restore();

            if (pattern.empty())
                notice = "E35: No previous regular expression";
            else
            {
                lastPattern = pattern;
                lastForward = searchForward;
                ops.jumpToMatch(
                    find(pattern, searchForward), pattern, searchForward);
            }
        }
        else
        {
            if (key == "Backspace" || key == "C-h")
            {
                if (command.empty())
                {
                    restore();
                    return Outcome::none;
                }

                command.erase(
                    static_cast<std::size_t>(previousChar(command, size(command))));
            }
            else if (isPrintable(key))
                command += key;

            auto found =
                command.empty() ? std::nullopt : find(command, searchForward);
            cursorRow = found ? found->at.line : searchStartRow;
            cursorColumn =
                found ? static_cast<int>(found->at.column) : searchStartColumn;
            liveMatch.reset();

            if (found)
                liveMatch = SearchMatch {found->at.line,
                                         static_cast<int>(found->at.column),
                                         static_cast<int>(found->length)};
        }

        return Outcome::none;
    }

    if (currentMode == Mode::command)
    {
        if (key == "Esc" || key == "C-c")
        {
            currentMode = Mode::normal;
            command.clear();
        }
        else if (key == "Backspace")
        {
            if (command.empty())
                currentMode = Mode::normal;
            else
                command.erase(
                    static_cast<std::size_t>(previousChar(command, size(command))));
        }
        else if (key == "Enter")
        {
            auto ran = command;
            command.clear();
            currentMode = Mode::normal;

            if (ran == "w")
            {
                markSaved();
                return Outcome::save;
            }

            if (ran == "wq" || ran == "x")
            {
                markSaved();
                return Outcome::saveAndClose;
            }

            if (ran == "q")
            {
                if (modified())
                {
                    notice = "E37: No write since last change (add ! to override)";
                    return Outcome::none;
                }

                return Outcome::close;
            }

            if (ran == "q!")
                return Outcome::discard;

            if (!ran.empty())
                notice = "E492: Not an editor command: " + ran;
        }
        else if (isPrintable(key))
        {
            command += key;
        }

        return Outcome::none;
    }

    if (currentMode == Mode::insert)
    {
        const auto& current = buffer.line(cursorRow);
        auto at =
            ::text::Position {cursorRow, static_cast<std::size_t>(cursorColumn)};

        if (key == "Esc" || key == "C-c")
        {
            currentMode = Mode::normal;
            cursorColumn = previousChar(current, cursorColumn);
            ops.clampNormal();
        }
        else if (key == "Enter")
            ops.insertRaw("\n");
        else if (key == "Tab")
            ops.insertRaw("    ");
        else if ((key == "C-w" || key == "C-u") && cursorColumn > 0)
        {
            auto from = 0;

            if (key == "C-w")
            {
                from = cursorColumn;

                while (from > 0
                       && classOf(current[static_cast<std::size_t>(from - 1)])
                              == CharClass::space)
                    --from;

                if (from > 0)
                {
                    auto cls = classOf(current[static_cast<std::size_t>(
                        previousChar(current, from))]);

                    while (from > 0)
                    {
                        auto before = previousChar(current, from);

                        if (classOf(current[static_cast<std::size_t>(before)])
                            != cls)
                            break;

                        from = before;
                    }
                }
            }

            buffer.erase({cursorRow, static_cast<std::size_t>(from)}, at);
            cursorColumn = from;
        }
        else if (key == "Backspace" || key == "C-h" || key == "C-w" || key == "C-u")
        {
            if (cursorColumn > 0)
            {
                auto from = previousChar(current, cursorColumn);
                buffer.erase({cursorRow, static_cast<std::size_t>(from)}, at);
                cursorColumn = from;
            }
            else if (cursorRow > 0)
            {
                auto previous = cursorRow - 1;
                auto join = size(buffer.line(previous));
                buffer.erase({previous, static_cast<std::size_t>(join)}, at);
                cursorRow = previous;
                cursorColumn = join;
            }
        }
        else if (key == "Delete")
        {
            if (cursorColumn < size(current))
            {
                auto to = nextChar(current, cursorColumn);
                buffer.erase(at, {cursorRow, static_cast<std::size_t>(to)});
            }
            else if (cursorRow + 1 < ops.rowCount())
            {
                buffer.erase(at, {cursorRow + 1, 0});
            }
        }
        else if (key == "Left")
            cursorColumn = previousChar(current, cursorColumn);
        else if (key == "Right")
            cursorColumn = nextChar(current, cursorColumn);
        else if (key == "Home")
            cursorColumn = 0;
        else if (key == "End")
            cursorColumn = size(current);
        else if (key == "Up" || key == "Down")
        {
            ops.moveVertical(key == "Up" ? -1 : 1);
            keepDesired = true;
        }
        else if (isPrintable(key))
            ops.insertRaw(key);

        if (!keepDesired)
            desiredColumn = -1;

        return Outcome::none;
    }

    if (key == "Esc" || key == "C-c")
    {
        pending.clear();
        return Outcome::none;
    }

    if (key == "C-r")
    {
        pending.clear();

        if (auto restored = buffer.redo(EditorOps::toText(ops.cursor())))
        {
            cursorRow = restored->line;
            cursorColumn = static_cast<int>(restored->column);
            ops.clampNormal();
        }

        return Outcome::none;
    }

    auto mapped = key;

    if (key == "Left" || key == "Backspace")
        mapped = "h";
    else if (key == "Right")
        mapped = "l";
    else if (key == "Up")
        mapped = "k";
    else if (key == "Down" || key == "Enter")
        mapped = "j";
    else if (key == "Home")
        mapped = "0";
    else if (key == "End")
        mapped = "$";
    else if (!isPrintable(key))
    {
        pending.clear();
        return Outcome::none;
    }

    pending += mapped;

    auto index = std::size_t {0};

    while (index < pending.size()
           && std::isdigit(static_cast<unsigned char>(pending[index])) != 0
           && !(index == 0 && pending[index] == '0'))
        ++index;

    auto hasCount = index > 0;
    auto count = hasCount ? std::stoi(pending.substr(0, index)) : 1;
    count = std::clamp(count, 1, 99999);
    auto rest = pending.substr(index);

    if (rest.empty())
        return Outcome::none; // still typing the count

    auto done = [&] { pending.clear(); };
    const auto& current = buffer.line(cursorRow);

    auto motion = [&](const std::string& name,
                      Position from) -> std::optional<Position>
    {
        auto p = from;

        for (auto n = 0; n < count; ++n)
        {
            if (name == "h")
                p.column = previousChar(ops.line(p.row), p.column);
            else if (name == "l")
                p.column = std::min(nextChar(ops.line(p.row), p.column),
                                    size(ops.line(p.row)));
            else if (name == "w")
                p = ops.wordForward(p);
            else if (name == "b")
                p = ops.wordBack(p);
            else if (name == "e")
                p = ops.wordEnd(p);
            else if (name == "0")
                p.column = 0;
            else if (name == "^")
                p.column = firstNonBlank(ops.line(p.row));
            else if (name == "$")
                p.column = size(ops.line(p.row));
            else
                return std::nullopt;
        }

        return p;
    };

    auto enterInsert = [&]
    {
        currentMode = Mode::insert;
        done();
    };

    if (rest == "h" || rest == "l" || rest == "w" || rest == "b" || rest == "e"
        || rest == "0" || rest == "^" || rest == "$")
    {
        auto target = *motion(rest, ops.cursor());
        ops.setCursor(target);
        ops.clampNormal();
        desiredColumn = -1;
        done();
        return Outcome::none;
    }

    if (rest == "j" || rest == "k")
    {
        ops.moveVertical(rest == "j" ? count : -count);
        ops.clampNormal();
        done();
        return Outcome::none; // keeps desiredColumn
    }

    desiredColumn = -1;

    if (rest == "g" || rest == "d" || rest == "c" || rest == "y" || rest == "r"
        || rest == "di" || rest == "da" || rest == "ci" || rest == "ca"
        || rest == "yi" || rest == "ya")
        return Outcome::none;

    if (rest == "gg" || rest == "G")
    {
        auto target = rest == "G" && !hasCount ? ops.rowCount() - 1 : count - 1;
        cursorRow = std::clamp(target, 0, ops.rowCount() - 1);
        cursorColumn = firstNonBlank(buffer.line(cursorRow));
        done();
        return Outcome::none;
    }

    if (rest == "x" || rest == "X")
    {
        if (!current.empty())
        {
            ops.pushUndo();

            if (rest == "x")
            {
                auto to = advance(current, cursorColumn, count, size(current));
                ops.erase(ops.cursor(), {cursorRow, to});
            }
            else
            {
                auto from = cursorColumn;

                for (auto n = 0; n < count && from > 0; ++n)
                    from = previousChar(current, from);

                ops.erase({cursorRow, from}, ops.cursor());
            }

            ops.clampNormal();
        }

        done();
        return Outcome::none;
    }

    if (rest == "i" || rest == "a" || rest == "I" || rest == "A" || rest == "o"
        || rest == "O")
    {
        ops.pushUndo();

        if (rest == "a" && !current.empty())
            cursorColumn = nextChar(current, cursorColumn);
        else if (rest == "I")
            cursorColumn = firstNonBlank(current) < size(current) && !current.empty()
                               ? firstNonBlank(current)
                               : 0;
        else if (rest == "A")
            cursorColumn = size(current);
        else if (rest == "o" || rest == "O")
        {
            auto at = rest == "o" ? cursorRow + 1 : cursorRow;
            buffer.insertLines(at, {std::string {}});
            cursorRow = at;
            cursorColumn = 0;
        }

        enterInsert();
        return Outcome::none;
    }

    if (rest == "D" || rest == "C")
    {
        ops.pushUndo();
        ops.erase(ops.cursor(), {cursorRow, size(current)});

        if (rest == "C")
            enterInsert();
        else
        {
            ops.clampNormal();
            done();
        }

        return Outcome::none;
    }

    if (rest == "J")
    {
        ops.pushUndo();
        ops.join(count);
        ops.clampNormal();
        done();
        return Outcome::none;
    }

    if (rest == "p" || rest == "P")
    {
        ops.put(rest == "p", count);
        ops.clampNormal();
        done();
        return Outcome::none;
    }

    if (rest == "u")
    {
        for (auto n = 0; n < count; ++n)
        {
            auto restored = buffer.undo(EditorOps::toText(ops.cursor()));

            if (!restored)
                break;

            cursorRow = restored->line;
            cursorColumn = static_cast<int>(restored->column);
        }

        ops.clampNormal();
        done();
        return Outcome::none;
    }

    if (rest == "~")
    {
        if (!current.empty())
        {
            ops.pushUndo();
            auto changed = current;

            for (auto n = 0; n < count && cursorColumn < size(changed); ++n)
            {
                auto& c = changed[static_cast<std::size_t>(cursorColumn)];
                auto byte = static_cast<unsigned char>(c);

                if (std::isupper(byte) != 0)
                    c = static_cast<char>(std::tolower(byte));
                else if (std::islower(byte) != 0)
                    c = static_cast<char>(std::toupper(byte));

                cursorColumn = nextChar(changed, cursorColumn);
            }

            buffer.replaceLine(cursorRow, std::move(changed));
            ops.clampNormal();
        }

        done();
        return Outcome::none;
    }

    if (rest == "/" || rest == "?")
    {
        currentMode = Mode::search;
        searchForward = rest == "/";
        searchStartRow = cursorRow;
        searchStartColumn = cursorColumn;
        command.clear();
        liveMatch.reset();
        done();
        return Outcome::none;
    }

    if (rest == "n" || rest == "N")
    {
        done();

        if (lastPattern.empty())
        {
            notice = "E35: No previous regular expression";
            return Outcome::none;
        }

        auto forward = rest == "n" ? lastForward : !lastForward;

        for (auto n = 0; n < count; ++n)
        {
            auto found = ::text::search(
                buffer.lines(),
                lastPattern,
                ::text::Position {cursorRow, static_cast<std::size_t>(cursorColumn)},
                forward);

            if (!ops.jumpToMatch(found, lastPattern, forward))
                break;
        }

        return Outcome::none;
    }

    if (rest == ":")
    {
        currentMode = Mode::command;
        command.clear();
        done();
        return Outcome::none;
    }

    if (rest.size() > 1 && rest[0] == 'r')
    {
        auto replacement = rest.substr(1);

        if (!current.empty() && isPrintable(replacement))
        {
            ops.pushUndo();
            auto to = nextChar(current, cursorColumn);
            auto changed = current;
            changed.replace(static_cast<std::size_t>(cursorColumn),
                            static_cast<std::size_t>(to - cursorColumn),
                            replacement);
            buffer.replaceLine(cursorRow, std::move(changed));
        }

        done();
        return Outcome::none;
    }

    // Operators: d c y followed by a motion, a text object or themselves.
    if (rest.size() >= 2 && (rest[0] == 'd' || rest[0] == 'c' || rest[0] == 'y'))
    {
        auto op = rest[0];
        auto target = rest.substr(1);
        done();

        if (target.size() == 1 && target[0] == op)
        {
            if (op == 'y')
            {
                ops.yankLines(count);
                return Outcome::none;
            }

            ops.pushUndo();

            if (op == 'd')
            {
                ops.deleteLines(count, true);
                return Outcome::none;
            }

            ops.yankLines(count);
            auto last = std::min(ops.rowCount(), cursorRow + count);
            buffer.eraseLines(cursorRow + 1, last - cursorRow - 1);
            buffer.replaceLine(cursorRow, {});
            cursorColumn = 0;
            currentMode = Mode::insert;
            return Outcome::none;
        }

        auto from = ops.cursor();
        auto to = from;

        if (target == "iw" || target == "aw")
        {
            auto [start, end] = target == "iw" ? ops.innerWord() : ops.aroundWord();
            from = {cursorRow, start};
            to = {cursorRow, end};
        }
        else
        {
            // cw acts like ce on a word, as in vim.
            auto name =
                op == 'c' && target == "w" && ops.classAt(from) != CharClass::space
                    ? std::string {"e"}
                    : target;
            auto moved = motion(name, from);

            if (!moved)
                return Outcome::none;

            to = *moved;

            if (name == "e")
                to.column = nextChar(ops.line(to.row), to.column); // inclusive

            if (name == "w" && to.row > from.row)
                to = {from.row, size(ops.line(from.row))};

            if (to < from)
                std::swap(from, to);
        }

        if (op == 'y')
        {
            yank = ops.extract(from, to);
            yankLinewise = false;
            return Outcome::none;
        }

        ops.pushUndo();
        ops.erase(from, to);

        if (op == 'c')
            currentMode = Mode::insert;
        else
            ops.clampNormal();

        return Outcome::none;
    }

    done();
    return Outcome::none;
}
} // namespace vim
