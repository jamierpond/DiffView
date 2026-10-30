#include <Highlight.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>

using namespace nano;
using namespace diffview;

namespace
{
std::vector<Token> tokenizeCpp(std::string_view text)
{
    auto carry = LexerState {};
    return tokenizeLine(text, Language::cFamily, carry);
}

std::optional<TokenKind> findKindOf(std::string_view text,
                                    const std::vector<Token>& tokens,
                                    std::string_view word)
{
    auto at = text.find(word);

    for (const auto& token: tokens)
        if (token.start == at && token.length == word.size())
            return token.kind;

    return std::nullopt;
}
} // namespace

auto tKeywordsAndCalls = test("Highlight/keywords and function calls") = []
{
    auto line = std::string_view {"return static_cast<int>(count(x));"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, "return") == TokenKind::keyword);
    check(findKindOf(line, tokens, "static_cast") == TokenKind::keyword);
    check(findKindOf(line, tokens, "count") == TokenKind::function);
    check(findKindOf(line, tokens, "x") == TokenKind::plain);
};

auto tNamespaceCall =
    test("Highlight/std::to_string is a namespace then a function") = []
{
    auto line = std::string_view {"auto s = std::to_string(n);"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, "std") == TokenKind::namespaceName);
    check(findKindOf(line, tokens, "to_string") == TokenKind::function);
};

auto tTypes = test("Highlight/std members and capitalised names are types") = []
{
    auto line = std::string_view {"std::string name; Hunk hunk;"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, "string") == TokenKind::type);
    check(findKindOf(line, tokens, "Hunk") == TokenKind::type);
};

auto tEscapedQuote =
    test("Highlight/a string with an escaped quote is one token") = []
{
    auto line = std::string_view {R"(auto s = "say \"hi\""; x)"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, R"("say \"hi\"")") == TokenKind::string);
    check(findKindOf(line, tokens, "x") == TokenKind::plain);
};

auto tLineComment = test("Highlight/a line comment runs to the end") = []
{
    auto line = std::string_view {"x = 1; // done"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, "// done") == TokenKind::comment);
    check(findKindOf(line, tokens, "1") == TokenKind::number);
};

auto tBlockComment = test("Highlight/a block comment carries to the next line") = []
{
    auto carry = LexerState {};
    auto first = tokenizeLine("int a; /* start", Language::cFamily, carry);

    check(carry.inBlockComment);
    check(first.back().kind == TokenKind::comment);

    auto secondLine = std::string_view {"end */ int b;"};
    auto second = tokenizeLine(secondLine, Language::cFamily, carry);

    check(!carry.inBlockComment);
    check(findKindOf(secondLine, second, "end */") == TokenKind::comment);
    check(findKindOf(secondLine, second, "int") == TokenKind::keyword);
};

auto tOpenerEndsLine =
    test("Highlight/an opener that ends the line is a token and carries") = []
{
    auto carry = LexerState {};
    auto comment = tokenizeLine("/**", Language::cFamily, carry);

    check(carry.inBlockComment);
    check(findKindOf("/**", comment, "/**") == TokenKind::comment);

    auto pythonCarry = LexerState {};
    auto quotes = tokenizeLine("x = \"\"\"", Language::python, pythonCarry);

    check(!pythonCarry.openStringEnd.empty());
    check(quotes.back().kind == TokenKind::string);

    auto rawCarry = LexerState {};
    auto raw = tokenizeLine("R\"x(", Language::cFamily, rawCarry);

    check(!rawCarry.openStringEnd.empty());
    check(findKindOf("R\"x(", raw, "R\"x(") == TokenKind::string);
};

auto tPreprocessor = test("Highlight/a preprocessor line is one token") = []
{
    auto tokens = tokenizeCpp("#include <vector>");

    check(tokens.size() == 1);
    check(tokens[0].kind == TokenKind::preprocessor);
};

auto tNumbers = test("Highlight/numbers") = []
{
    auto line = std::string_view {"f(0x1F, 2.5f, 1'000);"};
    auto tokens = tokenizeCpp(line);

    check(findKindOf(line, tokens, "0x1F") == TokenKind::number);
    check(findKindOf(line, tokens, "2.5f") == TokenKind::number);
    check(findKindOf(line, tokens, "1'000") == TokenKind::number);
};

auto tLanguageForPath = test("Highlight/languageForPath") = []
{
    check(languageForPath("Source/Render.cpp") == Language::cFamily);
    check(languageForPath("a/b.mm") == Language::cFamily);
    check(languageForPath("Tests/CMakeLists.txt") == Language::cmake);
    check(languageForPath("README.md") == Language::markdown);
    check(languageForPath("x.ts") == Language::javascript);
    check(languageForPath("notes.txt") == Language::plain);
    check(languageForPath("LICENSE") == Language::plain);
};

auto tPlainLanguage = test("Highlight/a plain file is one plain token") = []
{
    auto carry = LexerState {};
    auto tokens = tokenizeLine("return \"x\" // y", Language::plain, carry);

    check(tokens.size() == 1);
    check(tokens[0].kind == TokenKind::plain && tokens[0].length == 15);
};
