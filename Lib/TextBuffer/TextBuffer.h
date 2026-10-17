#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace text
{
struct Position
{
    int line = 0;
    std::size_t column = 0;

    bool operator==(const Position&) const = default;
};

// Lines of UTF-8 text; never fewer than one. Positions are clamped to the
// text, but keeping byte offsets on code point boundaries is the caller's job.
class TextBuffer
{
public:
    explicit TextBuffer(std::string text = {});

    // Forgets undo.
    void setText(std::string_view text);
    std::string text() const;

    const std::vector<std::string>& lines() const;
    int lineCount() const;
    const std::string& line(int index) const;

    // Returns the position just past the inserted text.
    Position insert(Position at, std::string_view text);

    std::string erase(Position from, Position to);

    void insertLines(int at, const std::vector<std::string>& lines);
    std::vector<std::string> eraseLines(int first, int count);

    void replaceLine(int index, std::string text);

    // snapshot() before a change. undo()/redo() take the cursor now (kept
    // for the other direction) and return the one saved with what they
    // restore.
    void snapshot(Position cursor);
    std::optional<Position> undo(Position cursor);
    std::optional<Position> redo(Position cursor);
    bool canUndo() const;
    bool canRedo() const;

private:
    Position clamp(Position at) const;

    struct Snapshot
    {
        std::vector<std::string> lines;
        Position cursor;
    };

    std::vector<std::string> buffer {std::string {}};
    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
};

// vim's smartcase: no uppercase letter in the needle means ignore case.
bool ignoresCase(std::string_view needle);

struct Match
{
    std::size_t start = 0;
    std::size_t length = 0;
};

// Non-overlapping, smartcase. Folding is ASCII-only, so a match never splits
// a multi-byte character.
std::vector<Match> findAll(std::string_view line, std::string_view needle);

struct SearchResult
{
    Position at;
    std::size_t length = 0;
    bool wrapped = false;
};

// Strictly after (or before) `from`, wrapping round; a match at `from`
// itself is only found by going all the way round.
std::optional<SearchResult> search(std::span<const std::string> lines,
                                   std::string_view needle,
                                   Position from,
                                   bool forward);
} // namespace text
