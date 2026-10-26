#pragma once

#include "Diff.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
enum class TokenKind
{
    plain,
    keyword,
    type,
    function,
    namespaceName,
    string,
    number,
    comment,
    preprocessor,
    punctuation
};

// Byte range within one line's text.
struct Token
{
    std::size_t start = 0;
    std::size_t length = 0;
    TokenKind kind = TokenKind::plain;
};

enum class Language
{
    plain,
    cFamily,
    cmake,
    shell,
    python,
    javascript,
    json,
    markdown,
    swift,
    rust,
    go,
    java,
    yaml
};

// What a line leaves open for the next one.
struct LexerState
{
    bool inBlockComment = false;
    bool inCodeFence = false;

    // Closes a string still open at the end of the line: `)delim"`, `"""`, a backtick.
    std::string openStringEnd;
};

Language languageForPath(std::string_view path);

std::vector<Token>
    tokenizeLine(std::string_view text, Language language, LexerState& carry);

// [hunk][line], each tokenised over displayable(line.text).
using FileTokens = std::vector<std::vector<std::vector<Token>>>;

// Old and new sides are lexed apart so a comment opened on one side does not
// leak into the other; context lines take the new side's tokens.
FileTokens highlightFile(const FileDiff& file);
} // namespace diffview
