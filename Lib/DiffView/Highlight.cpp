#include "Highlight.h"

#include "Text.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace diffview
{
namespace
{
using Keywords = std::unordered_set<std::string_view>;

struct LanguageRules
{
    std::string_view lineComment;
    std::string_view blockOpen;
    std::string_view blockClose;
    const Keywords& keywords;
    std::string_view quotes = "\"'";
    bool hasPreprocessor = false;
    bool hasRawStrings = false;
    bool hasTripleQuotes = false;
    bool capitalisedIsType = true;
};

const Keywords cFamilyKeywords {"alignas",
                                "alignof",
                                "asm",
                                "auto",
                                "bool",
                                "break",
                                "case",
                                "catch",
                                "char",
                                "class",
                                "co_await",
                                "co_return",
                                "co_yield",
                                "concept",
                                "const",
                                "consteval",
                                "constexpr",
                                "constinit",
                                "const_cast",
                                "continue",
                                "decltype",
                                "default",
                                "delete",
                                "do",
                                "double",
                                "dynamic_cast",
                                "else",
                                "enum",
                                "explicit",
                                "export",
                                "extern",
                                "false",
                                "final",
                                "float",
                                "for",
                                "friend",
                                "goto",
                                "if",
                                "inline",
                                "int",
                                "long",
                                "mutable",
                                "namespace",
                                "new",
                                "noexcept",
                                "nullptr",
                                "operator",
                                "override",
                                "private",
                                "protected",
                                "public",
                                "register",
                                "reinterpret_cast",
                                "requires",
                                "return",
                                "short",
                                "signed",
                                "sizeof",
                                "static",
                                "static_assert",
                                "static_cast",
                                "struct",
                                "switch",
                                "template",
                                "this",
                                "thread_local",
                                "throw",
                                "true",
                                "try",
                                "typedef",
                                "typeid",
                                "typename",
                                "union",
                                "unsigned",
                                "using",
                                "virtual",
                                "void",
                                "volatile",
                                "while",
                                "self",
                                "nil",
                                "YES",
                                "NO",
                                "id",
                                "@interface",
                                "@implementation",
                                "@end",
                                "@property"};

const Keywords cmakeKeywords {
    "if",     "elseif",   "else",     "endif",       "foreach", "endforeach",
    "while",  "endwhile", "function", "endfunction", "macro",   "endmacro",
    "return", "PRIVATE",  "PUBLIC",   "INTERFACE",   "STATIC",  "SHARED",
    "AND",    "OR",       "NOT"};

const Keywords shellKeywords {
    "if",     "then",     "else",  "elif",  "fi",   "for",      "in",       "do",
    "done",   "while",    "until", "case",  "esac", "function", "return",   "local",
    "export", "readonly", "set",   "unset", "exit", "break",    "continue", "source",
    "echo",   "cd",       "shift", "true",  "false"};

const Keywords pythonKeywords {
    "False",  "None",     "True",  "and",    "as",       "assert",
    "async",  "await",    "break", "class",  "continue", "def",
    "del",    "elif",     "else",  "except", "finally",  "for",
    "from",   "global",   "if",    "import", "in",       "is",
    "lambda", "nonlocal", "not",   "or",     "pass",     "raise",
    "return", "try",      "while", "with",   "yield",    "self"};

const Keywords javascriptKeywords {
    "async",      "await",     "break",      "case",    "catch",     "class",
    "const",      "continue",  "debugger",   "default", "delete",    "do",
    "else",       "enum",      "export",     "extends", "false",     "finally",
    "for",        "from",      "function",   "if",      "import",    "in",
    "instanceof", "interface", "let",        "new",     "null",      "of",
    "return",     "static",    "super",      "switch",  "this",      "throw",
    "true",       "try",       "type",       "typeof",  "undefined", "var",
    "void",       "while",     "with",       "yield",   "readonly",  "public",
    "private",    "protected", "implements", "as"};

const Keywords jsonKeywords {"true", "false", "null"};

const Keywords swiftKeywords {"associatedtype",
                              "break",
                              "case",
                              "catch",
                              "class",
                              "continue",
                              "default",
                              "defer",
                              "deinit",
                              "do",
                              "else",
                              "enum",
                              "extension",
                              "fallthrough",
                              "false",
                              "fileprivate",
                              "for",
                              "func",
                              "guard",
                              "if",
                              "import",
                              "in",
                              "init",
                              "inout",
                              "internal",
                              "is",
                              "let",
                              "nil",
                              "open",
                              "operator",
                              "private",
                              "protocol",
                              "public",
                              "repeat",
                              "rethrows",
                              "return",
                              "self",
                              "Self",
                              "some",
                              "static",
                              "struct",
                              "subscript",
                              "super",
                              "switch",
                              "throw",
                              "throws",
                              "true",
                              "try",
                              "typealias",
                              "var",
                              "where",
                              "while",
                              "async",
                              "await",
                              "override",
                              "final",
                              "weak",
                              "lazy",
                              "mutating"};

const Keywords rustKeywords {
    "as",   "async",  "await",  "break", "const",  "continue", "crate", "dyn",
    "else", "enum",   "extern", "false", "fn",     "for",      "if",    "impl",
    "in",   "let",    "loop",   "match", "mod",    "move",     "mut",   "pub",
    "ref",  "return", "self",   "Self",  "static", "struct",   "super", "trait",
    "true", "type",   "unsafe", "use",   "where",  "while"};

const Keywords goKeywords {
    "break",  "case",        "chan", "const",   "continue", "default", "defer",
    "else",   "fallthrough", "for",  "func",    "go",       "goto",    "if",
    "import", "interface",   "map",  "package", "range",    "return",  "select",
    "struct", "switch",      "type", "var",     "true",     "false",   "nil"};

const Keywords javaKeywords {
    "abstract",   "assert",    "boolean",   "break",        "byte",       "case",
    "catch",      "char",      "class",     "const",        "continue",   "default",
    "do",         "double",    "else",      "enum",         "extends",    "final",
    "finally",    "float",     "for",       "if",           "implements", "import",
    "instanceof", "int",       "interface", "long",         "native",     "new",
    "package",    "private",   "protected", "public",       "return",     "short",
    "static",     "super",     "switch",    "synchronized", "this",       "throw",
    "throws",     "transient", "try",       "void",         "volatile",   "while",
    "true",       "false",     "null",      "fun",          "val",        "var",
    "object",     "when",      "is",        "in",           "override",   "data",
    "companion",  "suspend",   "internal",  "open",         "lateinit"};

const Keywords yamlKeywords {"true", "false", "null", "yes", "no", "on", "off"};

const Keywords noKeywords {};

LanguageRules rulesFor(Language language)
{
    switch (language)
    {
        case Language::cFamily:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = cFamilyKeywords,
                    .hasPreprocessor = true,
                    .hasRawStrings = true};
        case Language::cmake:
            return {.lineComment = "#",
                    .keywords = cmakeKeywords,
                    .quotes = "\"",
                    .capitalisedIsType = false};
        case Language::shell:
            return {.lineComment = "#",
                    .keywords = shellKeywords,
                    .capitalisedIsType = false};
        case Language::python:
            return {.lineComment = "#",
                    .keywords = pythonKeywords,
                    .hasTripleQuotes = true};
        case Language::javascript:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = javascriptKeywords,
                    .quotes = "\"'`"};
        case Language::json:
            return {.keywords = jsonKeywords,
                    .quotes = "\"",
                    .capitalisedIsType = false};
        case Language::swift:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = swiftKeywords,
                    .quotes = "\"",
                    .hasTripleQuotes = true};
        case Language::rust:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = rustKeywords,
                    .quotes = "\""};
        case Language::go:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = goKeywords,
                    .quotes = "\"'`"};
        case Language::java:
            return {.lineComment = "//",
                    .blockOpen = "/*",
                    .blockClose = "*/",
                    .keywords = javaKeywords,
                    .quotes = "\"'",
                    .hasTripleQuotes = true};
        case Language::yaml:
            return {.lineComment = "#",
                    .keywords = yamlKeywords,
                    .capitalisedIsType = false};
        case Language::markdown:
        case Language::plain:
            break;
    }

    return {.keywords = noKeywords, .quotes = "", .capitalisedIsType = false};
}

bool isIdentifierStart(char c)
{
    auto byte = static_cast<unsigned char>(c);
    return std::isalpha(byte) != 0 || c == '_' || c == '$' || c == '@'
           || byte >= 0x80;
}

bool isIdentifierPart(char c)
{
    auto byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) != 0 || c == '_' || c == '$' || byte >= 0x80;
}

bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t';
}

std::size_t skipSpaces(std::string_view text, std::size_t from)
{
    while (from < text.size() && isSpace(text[from]))
        ++from;

    return from;
}

bool startsAt(std::string_view text, std::size_t at, std::string_view prefix)
{
    return !prefix.empty() && text.substr(at).starts_with(prefix);
}

class LineLexer
{
public:
    LineLexer(std::string_view textToLex,
              const LanguageRules& languageRules,
              LexerState& state)
        : text(textToLex)
        , rules(languageRules)
        , carry(state)
    {
    }

    std::vector<Token> tokenize()
    {
        if (!carry.openStringEnd.empty())
            continueOpenString();

        if (carry.inBlockComment)
            continueBlockComment();

        if (rules.hasPreprocessor && position == 0)
        {
            auto first = skipSpaces(text, 0);

            if (first < text.size() && text[first] == '#')
            {
                emit(first, text.size(), TokenKind::preprocessor);
                return tokens;
            }
        }

        while (position < text.size())
            lexNext();

        return tokens;
    }

private:
    void emit(std::size_t start, std::size_t end, TokenKind kind)
    {
        if (end > start)
            tokens.push_back({start, end - start, kind});

        position = end;
    }

    void continueOpenString()
    {
        auto start = position;
        emit(start, endOfOpenString(), TokenKind::string);
    }

    void continueBlockComment()
    {
        auto start = position;
        emit(start, endOfBlockComment(), TokenKind::comment);
    }

    std::size_t endOfOpenString()
    {
        auto close = text.find(carry.openStringEnd, position);

        if (close == std::string_view::npos)
            return text.size();

        auto end = close + carry.openStringEnd.size();
        carry.openStringEnd.clear();
        return end;
    }

    std::size_t endOfBlockComment()
    {
        auto close = text.find(rules.blockClose, position);

        if (close == std::string_view::npos)
            return text.size();

        carry.inBlockComment = false;
        return close + rules.blockClose.size();
    }

    void lexNext()
    {
        auto c = text[position];

        if (isSpace(c))
        {
            position = skipSpaces(text, position);
            return;
        }

        if (startsAt(text, position, rules.lineComment))
        {
            emit(position, text.size(), TokenKind::comment);
            return;
        }

        if (startsAt(text, position, rules.blockOpen))
        {
            auto start = position;
            carry.inBlockComment = true;
            position += rules.blockOpen.size();
            emit(start, endOfBlockComment(), TokenKind::comment);
            return;
        }

        if (rules.hasRawStrings && startsAt(text, position, "R\""))
        {
            lexRawString();
            return;
        }

        if (rules.hasTripleQuotes
            && (startsAt(text, position, "\"\"\"")
                || startsAt(text, position, "'''")))
        {
            auto start = position;
            carry.openStringEnd = std::string(text.substr(position, 3));
            position += 3;
            emit(start, endOfOpenString(), TokenKind::string);
            return;
        }

        if (rules.quotes.find(c) != std::string_view::npos)
        {
            lexQuoted(c);
            return;
        }

        if (isDigit(c)
            || (c == '.' && position + 1 < text.size()
                && isDigit(text[position + 1])))
        {
            lexNumber();
            return;
        }

        if (isIdentifierStart(c))
        {
            lexIdentifier();
            return;
        }

        emit(position, position + 1, TokenKind::punctuation);
    }

    void lexRawString()
    {
        auto start = position;
        auto open = text.find('(', position + 2);

        if (open == std::string_view::npos)
        {
            emit(start, text.size(), TokenKind::string);
            return;
        }

        auto delimiter = text.substr(position + 2, open - position - 2);
        carry.openStringEnd = ")" + std::string(delimiter) + "\"";
        position = open + 1;
        emit(start, endOfOpenString(), TokenKind::string);
    }

    void lexQuoted(char quote)
    {
        auto start = position;
        auto end = position + 1;

        while (end < text.size() && text[end] != quote)
            end += text[end] == '\\' ? 2 : 1;

        if (end >= text.size() && quote == '`')
        {
            carry.openStringEnd = "`";
            emit(start, text.size(), TokenKind::string);
            return;
        }

        end = std::min(end + 1, text.size());
        auto next = skipSpaces(text, end);
        auto isKey = rules.quotes == "\"" && rules.lineComment.empty()
                     && next < text.size() && text[next] == ':';
        emit(start, end, isKey ? TokenKind::type : TokenKind::string);
    }

    void lexNumber()
    {
        auto end = position;

        while (end < text.size()
               && (isIdentifierPart(text[end]) || text[end] == '.'
                   || text[end] == '\''))
            ++end;

        emit(position, end, TokenKind::number);
    }

    void lexIdentifier()
    {
        auto start = position;
        auto end = position + 1;

        while (end < text.size() && isIdentifierPart(text[end]))
            ++end;

        auto word = text.substr(start, end - start);
        auto next = skipSpaces(text, end);
        emit(start, end, classifyIdentifier(word, start, next));
    }

    TokenKind classifyIdentifier(std::string_view word,
                                 std::size_t start,
                                 std::size_t next) const
    {
        if (rules.keywords.contains(word))
            return TokenKind::keyword;
        if (startsAt(text, next, "::"))
            return TokenKind::namespaceName;
        if (next < text.size() && text[next] == '(')
            return TokenKind::function;
        if (start >= 5 && text.substr(start - 5, 5) == "std::")
            return TokenKind::type;
        if (rules.capitalisedIsType
            && std::isupper(static_cast<unsigned char>(word[0])) != 0)
            return TokenKind::type;

        return TokenKind::plain;
    }

    std::string_view text;
    const LanguageRules& rules;
    LexerState& carry;
    std::size_t position = 0;
    std::vector<Token> tokens;
};

std::vector<Token> tokenizeMarkdownLine(std::string_view text, LexerState& carry)
{
    auto first = skipSpaces(text, 0);

    if (startsAt(text, first, "```"))
    {
        carry.inCodeFence = !carry.inCodeFence;
        return {{first, text.size() - first, TokenKind::string}};
    }

    if (carry.inCodeFence)
        return {{0, text.size(), TokenKind::string}};

    if (startsAt(text, first, "#"))
        return {{first, text.size() - first, TokenKind::keyword}};

    return {{0, text.size(), TokenKind::plain}};
}

// "  - name: value" marks `name` as a key, then lexes the rest.
std::vector<Token> tokenizeYamlLine(std::string_view text, LexerState& carry)
{
    auto keyStart = skipSpaces(text, 0);

    if (startsAt(text, keyStart, "- "))
        keyStart = skipSpaces(text, keyStart + 2);

    auto keyEnd = keyStart;

    while (keyEnd < text.size()
           && (isIdentifierPart(text[keyEnd]) || text[keyEnd] == '-'
               || text[keyEnd] == '.'))
        ++keyEnd;

    auto isKey = keyEnd > keyStart && keyEnd < text.size() && text[keyEnd] == ':';

    if (!isKey)
        return LineLexer(text, rulesFor(Language::yaml), carry).tokenize();

    auto tokens =
        std::vector<Token> {{keyStart, keyEnd - keyStart, TokenKind::type}};
    auto restStart = keyEnd + 1;
    auto rest = LineLexer(text.substr(restStart), rulesFor(Language::yaml), carry)
                    .tokenize();

    for (auto token: rest)
    {
        token.start += restStart;
        tokens.push_back(token);
    }

    return tokens;
}

struct ExtensionLanguage
{
    std::string_view extension;
    Language language;
};

constexpr auto extensionLanguages =
    std::array {ExtensionLanguage {".c", Language::cFamily},
                ExtensionLanguage {".cc", Language::cFamily},
                ExtensionLanguage {".cpp", Language::cFamily},
                ExtensionLanguage {".cxx", Language::cFamily},
                ExtensionLanguage {".h", Language::cFamily},
                ExtensionLanguage {".hh", Language::cFamily},
                ExtensionLanguage {".hpp", Language::cFamily},
                ExtensionLanguage {".mm", Language::cFamily},
                ExtensionLanguage {".m", Language::cFamily},
                ExtensionLanguage {".inl", Language::cFamily},
                ExtensionLanguage {".cmake", Language::cmake},
                ExtensionLanguage {".sh", Language::shell},
                ExtensionLanguage {".bash", Language::shell},
                ExtensionLanguage {".zsh", Language::shell},
                ExtensionLanguage {".py", Language::python},
                ExtensionLanguage {".js", Language::javascript},
                ExtensionLanguage {".mjs", Language::javascript},
                ExtensionLanguage {".cjs", Language::javascript},
                ExtensionLanguage {".jsx", Language::javascript},
                ExtensionLanguage {".ts", Language::javascript},
                ExtensionLanguage {".tsx", Language::javascript},
                ExtensionLanguage {".json", Language::json},
                ExtensionLanguage {".md", Language::markdown},
                ExtensionLanguage {".swift", Language::swift},
                ExtensionLanguage {".rs", Language::rust},
                ExtensionLanguage {".go", Language::go},
                ExtensionLanguage {".java", Language::java},
                ExtensionLanguage {".kt", Language::java},
                ExtensionLanguage {".kts", Language::java},
                ExtensionLanguage {".yml", Language::yaml},
                ExtensionLanguage {".yaml", Language::yaml}};

std::string_view findFileName(std::string_view path)
{
    auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}
} // namespace

Language languageForPath(std::string_view path)
{
    auto name = findFileName(path);

    if (name == "CMakeLists.txt")
        return Language::cmake;
    if (name == "justfile" || name == "Justfile" || name == "Makefile")
        return Language::shell;

    auto dot = name.rfind('.');

    if (dot == std::string_view::npos)
        return Language::plain;

    auto extension = name.substr(dot);

    for (const auto& entry: extensionLanguages)
        if (entry.extension == extension)
            return entry.language;

    return Language::plain;
}

std::vector<Token>
    tokenizeLine(std::string_view text, Language language, LexerState& carry)
{
    if (text.empty())
        return {};

    switch (language)
    {
        case Language::plain:
            return {{0, text.size(), TokenKind::plain}};
        case Language::markdown:
            return tokenizeMarkdownLine(text, carry);
        case Language::yaml:
            return tokenizeYamlLine(text, carry);
        default:
            break;
    }

    return LineLexer(text, rulesFor(language), carry).tokenize();
}

FileTokens highlightFile(const FileDiff& file)
{
    auto language = languageForPath(file.path);
    auto oldSide = LexerState {};
    auto newSide = LexerState {};
    auto tokens = FileTokens {};
    tokens.reserve(file.hunks.size());

    for (const auto& hunk: file.hunks)
    {
        auto& hunkTokens = tokens.emplace_back();
        hunkTokens.reserve(hunk.lines.size());

        for (const auto& line: hunk.lines)
        {
            auto shown = displayable(line.text);

            if (line.kind == LineKind::removed)
            {
                hunkTokens.push_back(tokenizeLine(shown, language, oldSide));
                continue;
            }

            if (line.kind == LineKind::context)
                tokenizeLine(shown, language, oldSide);

            hunkTokens.push_back(tokenizeLine(shown, language, newSide));
        }
    }

    return tokens;
}
} // namespace diffview
