#include "Text.h"

#include <cctype>

namespace diffview
{
namespace
{
bool isContinuationByte(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

std::size_t advance(std::string_view text, std::size_t from, int count)
{
    auto pos = from;

    while (pos < text.size() && count > 0)
    {
        ++pos;

        while (pos < text.size() && isContinuationByte(text[pos]))
            ++pos;

        --count;
    }

    return pos;
}
} // namespace

std::vector<std::string> splitLines(std::string_view text)
{
    auto lines = std::vector<std::string> {};
    auto start = std::size_t {0};

    while (start < text.size())
    {
        auto end = text.find('\n', start);

        if (end == std::string_view::npos)
        {
            lines.emplace_back(text.substr(start));
            break;
        }

        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }

    return lines;
}

std::vector<std::string> splitNul(std::string_view text)
{
    auto parts = std::vector<std::string> {};
    auto start = std::size_t {0};

    while (start < text.size())
    {
        auto end = text.find('\0', start);

        if (end == std::string_view::npos)
            end = text.size();

        parts.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }

    return parts;
}

std::string trim(std::string_view text)
{
    auto isSpace = [](char c)
    { return std::isspace(static_cast<unsigned char>(c)) != 0; };

    while (!text.empty() && isSpace(text.front()))
        text.remove_prefix(1);

    while (!text.empty() && isSpace(text.back()))
        text.remove_suffix(1);

    return std::string {text};
}

std::string toLower(std::string_view text)
{
    auto lower = std::string {text};

    for (auto& c: lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return lower;
}

std::string displayable(std::string_view line, int tabWidth)
{
    auto out = std::string {};
    out.reserve(line.size());
    auto column = 0;

    for (auto c: line)
    {
        if (c == '\t')
        {
            auto spaces = tabWidth - (column % tabWidth);
            out.append(static_cast<std::size_t>(spaces), ' ');
            column += spaces;
            continue;
        }

        auto byte = static_cast<unsigned char>(c);

        if (byte < 0x20 || byte == 0x7F)
            out.push_back('?');
        else
            out.push_back(c);

        if (!isContinuationByte(c))
            ++column;
    }

    return out;
}

int codepointCount(std::string_view text)
{
    auto count = 0;

    for (auto c: text)
        if (!isContinuationByte(c))
            ++count;

    return count;
}

std::vector<std::string> wrap(std::string_view line, int width)
{
    if (width < 1)
        width = 1;

    auto pieces = std::vector<std::string> {};

    while (codepointCount(line) > width)
    {
        auto cut = advance(line, 0, width);
        auto space = line.substr(0, cut).rfind(' ');

        if (space != std::string_view::npos && space > 0)
        {
            pieces.emplace_back(line.substr(0, space));
            line.remove_prefix(space + 1);
        }
        else
        {
            pieces.emplace_back(line.substr(0, cut));
            line.remove_prefix(cut);
        }
    }

    pieces.emplace_back(line);
    return pieces;
}

std::uint64_t fnv1a(std::string_view text, std::uint64_t seed)
{
    auto hash = seed;

    for (auto c: text)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }

    return hash;
}
} // namespace diffview
