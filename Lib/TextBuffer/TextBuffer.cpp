#include "TextBuffer.h"

#include <algorithm>

namespace text
{
namespace
{
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

char foldAscii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}
} // namespace

TextBuffer::TextBuffer(std::string text)
{
    setText(text);
}

void TextBuffer::setText(std::string_view text)
{
    buffer = splitText(text);
    undoStack.clear();
    redoStack.clear();
}

std::string TextBuffer::text() const
{
    auto out = std::string {};

    for (std::size_t i = 0; i < buffer.size(); ++i)
        out += (i == 0 ? "" : "\n") + buffer[i];

    return out;
}

const std::vector<std::string>& TextBuffer::lines() const
{
    return buffer;
}

int TextBuffer::lineCount() const
{
    return static_cast<int>(buffer.size());
}

const std::string& TextBuffer::line(int index) const
{
    return buffer[static_cast<std::size_t>(std::clamp(index, 0, lineCount() - 1))];
}

Position TextBuffer::clamp(Position at) const
{
    at.line = std::clamp(at.line, 0, lineCount() - 1);
    at.column = std::min(at.column, line(at.line).size());
    return at;
}

Position TextBuffer::insert(Position at, std::string_view text)
{
    at = clamp(at);
    auto pieces = splitText(text);
    auto& first = buffer[static_cast<std::size_t>(at.line)];
    auto tail = first.substr(at.column);
    first.resize(at.column);
    first += pieces.front();

    if (pieces.size() == 1)
    {
        auto end = Position {at.line, first.size()};
        first += tail;
        return end;
    }

    auto row = at.line;

    for (std::size_t i = 1; i < pieces.size(); ++i)
        buffer.insert(buffer.begin() + ++row, pieces[i]);

    auto& last = buffer[static_cast<std::size_t>(row)];
    auto end = Position {row, last.size()};
    last += tail;
    return end;
}

std::string TextBuffer::erase(Position from, Position to)
{
    from = clamp(from);
    to = clamp(to);

    if (to.line < from.line || (to.line == from.line && to.column < from.column))
        std::swap(from, to);

    if (from == to)
        return {};

    auto& first = buffer[static_cast<std::size_t>(from.line)];

    if (from.line == to.line)
    {
        auto removed = first.substr(from.column, to.column - from.column);
        first.erase(from.column, to.column - from.column);
        return removed;
    }

    auto removed = first.substr(from.column);

    for (auto r = from.line + 1; r < to.line; ++r)
        removed += "\n" + buffer[static_cast<std::size_t>(r)];

    const auto& last = buffer[static_cast<std::size_t>(to.line)];
    removed += "\n" + last.substr(0, to.column);

    auto tail = last.substr(to.column);
    first.resize(from.column);
    first += tail;
    buffer.erase(buffer.begin() + from.line + 1, buffer.begin() + to.line + 1);
    return removed;
}

void TextBuffer::insertLines(int at, const std::vector<std::string>& lines)
{
    at = std::clamp(at, 0, lineCount());
    buffer.insert(buffer.begin() + at, lines.begin(), lines.end());
}

std::vector<std::string> TextBuffer::eraseLines(int first, int count)
{
    first = std::clamp(first, 0, lineCount() - 1);
    auto last = std::clamp(first + count, first, lineCount());
    auto removed =
        std::vector<std::string>(buffer.begin() + first, buffer.begin() + last);
    buffer.erase(buffer.begin() + first, buffer.begin() + last);

    if (buffer.empty())
        buffer.emplace_back();

    return removed;
}

void TextBuffer::replaceLine(int index, std::string text)
{
    buffer[static_cast<std::size_t>(std::clamp(index, 0, lineCount() - 1))] =
        std::move(text);
}

void TextBuffer::snapshot(Position cursor)
{
    undoStack.push_back({buffer, cursor});
    redoStack.clear();
}

std::optional<Position> TextBuffer::undo(Position cursor)
{
    if (undoStack.empty())
        return std::nullopt;

    redoStack.push_back({buffer, cursor});
    auto restored = std::move(undoStack.back());
    undoStack.pop_back();
    buffer = std::move(restored.lines);
    return clamp(restored.cursor);
}

std::optional<Position> TextBuffer::redo(Position cursor)
{
    if (redoStack.empty())
        return std::nullopt;

    undoStack.push_back({buffer, cursor});
    auto restored = std::move(redoStack.back());
    redoStack.pop_back();
    buffer = std::move(restored.lines);
    return clamp(restored.cursor);
}

bool TextBuffer::canUndo() const
{
    return !undoStack.empty();
}

bool TextBuffer::canRedo() const
{
    return !redoStack.empty();
}

bool ignoresCase(std::string_view needle)
{
    return std::none_of(
        needle.begin(), needle.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
}

std::vector<Match> findAll(std::string_view line, std::string_view needle)
{
    auto matches = std::vector<Match> {};

    if (needle.empty() || needle.size() > line.size())
        return matches;

    auto fold = ignoresCase(needle);
    auto equal = [fold](char a, char b)
    { return fold ? foldAscii(a) == foldAscii(b) : a == b; };

    auto at = std::size_t {0};

    while (at + needle.size() <= line.size())
    {
        auto hit = std::equal(needle.begin(),
                              needle.end(),
                              line.begin() + static_cast<std::ptrdiff_t>(at),
                              equal);

        if (hit)
        {
            matches.push_back({at, needle.size()});
            at += needle.size();
        }
        else
        {
            ++at;
        }
    }

    return matches;
}

std::optional<SearchResult> search(std::span<const std::string> lines,
                                   std::string_view needle,
                                   Position from,
                                   bool forward)
{
    if (lines.empty() || needle.empty())
        return std::nullopt;

    auto count = static_cast<int>(lines.size());
    from.line = std::clamp(from.line, 0, count - 1);

    auto matchesOn = [&](int line)
    { return findAll(lines[static_cast<std::size_t>(line)], needle); };

    auto result = [&](int line, const Match& match, bool wrapped)
    { return SearchResult {{line, match.start}, match.length, wrapped}; };

    if (forward)
    {
        for (const auto& match: matchesOn(from.line))
            if (match.start > from.column)
                return result(from.line, match, false);

        for (auto line = from.line + 1; line < count; ++line)
            if (auto found = matchesOn(line); !found.empty())
                return result(line, found.front(), false);

        for (auto line = 0; line < from.line; ++line)
            if (auto found = matchesOn(line); !found.empty())
                return result(line, found.front(), true);

        for (const auto& match: matchesOn(from.line))
            if (match.start <= from.column)
                return result(from.line, match, true);

        return std::nullopt;
    }

    auto before = matchesOn(from.line);

    for (auto it = before.rbegin(); it != before.rend(); ++it)
        if (it->start < from.column)
            return result(from.line, *it, false);

    for (auto line = from.line - 1; line >= 0; --line)
        if (auto found = matchesOn(line); !found.empty())
            return result(line, found.back(), false);

    for (auto line = count - 1; line > from.line; --line)
        if (auto found = matchesOn(line); !found.empty())
            return result(line, found.back(), true);

    for (auto it = before.rbegin(); it != before.rend(); ++it)
        if (it->start >= from.column)
            return result(from.line, *it, true);

    return std::nullopt;
}
} // namespace text
