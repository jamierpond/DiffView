#pragma once

#include <TextBuffer.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vim
{
enum class Mode
{
    normal,
    insert,
    command,
    search
};

// Saving, closing and the clipboard belong to the host; feed() says when
// it's asked for one.
enum class Outcome
{
    none,
    save,
    saveAndClose,
    close,
    discard,
    paste // the host reads the clipboard and calls insertText
};

// A small vim: the model and its keys, no drawing. A key is one UTF-8
// character or a name: "Esc", "Enter", "Backspace", "Delete", "Tab", the
// arrows, "Home", "End", "C-s", "C-v", "C-c", "C-r", "C-w", "C-u", "C-h".
// The cursor is a byte offset on a code point boundary. With a wrap width,
// j/k move by displayed row so a long paragraph can be walked line by line.
class Editor
{
public:
    explicit Editor(std::string text = {});

    Outcome feed(const std::string& key);

    std::string text() const;

    // Counts as saved; clears undo.
    void setText(std::string text);

    // One undo step; the mode is left as it was.
    void insertText(std::string_view text);

    bool modified() const;
    void markSaved();

    Mode mode() const;
    const std::vector<std::string>& lines() const;
    int row() const;
    int column() const;

    // What's typed after the prompt character: an ex command or a pattern.
    std::string commandLine() const;

    // ':', '/' or '?' while a prompt is open, else 0.
    char promptChar() const;

    struct SearchMatch
    {
        int row = 0;
        int column = 0;
        int length = 0;
    };

    // The incremental match while a search pattern is being typed.
    std::optional<SearchMatch> searchMatch() const;

    // Cleared by the next key.
    std::string message() const;

    // 0 wraps nothing.
    void setWrapWidth(int columns);

    // One displayed row: bytes [start, end) of lines[line].
    struct VisualRow
    {
        int line = 0;
        int start = 0;
        int end = 0;
    };

    std::vector<VisualRow> visualRows() const;
    int cursorVisualRow() const;

    // In code points.
    int cursorVisualColumn() const;

private:
    friend struct EditorOps;

    ::text::TextBuffer buffer;
    int cursorRow = 0;
    int cursorColumn = 0;
    int wrapWidth = 0;
    Mode currentMode = Mode::normal;
    std::string command;
    std::string notice;
    std::string pending; // count and operator typed so far: "2d", "ci", "g"
    std::string savedText;
    std::string yank;
    bool yankLinewise = false;
    int desiredColumn = -1;

    bool searchForward = true;
    int searchStartRow = 0;
    int searchStartColumn = 0;
    std::optional<SearchMatch> liveMatch;
    std::string lastPattern;
    bool lastForward = true;
};
} // namespace vim
