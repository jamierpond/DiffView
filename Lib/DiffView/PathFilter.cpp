#include "PathFilter.h"

#include <algorithm>
#include <cctype>

namespace diffview
{
namespace
{
bool hasUppercase(const std::string& text)
{
    return std::any_of(text.begin(),
                       text.end(),
                       [](char c)
                       { return std::isupper(static_cast<unsigned char>(c)) != 0; });
}
} // namespace

bool PathFilter::matches(const std::string& path) const
{
    return std::regex_search(path, expression) != inverted;
}

std::optional<PathFilter> parsePathFilter(const std::string& pattern)
{
    auto inverted = pattern.starts_with("!");
    auto body = inverted ? pattern.substr(1) : pattern;

    if (body.empty())
        return std::nullopt;

    auto flags = std::regex::ECMAScript;

    if (!hasUppercase(body))
        flags |= std::regex::icase;

    try
    {
        return PathFilter {pattern, std::regex(body, flags), inverted};
    }
    catch (const std::regex_error&)
    {
        return std::nullopt;
    }
}

std::vector<std::string> filterPaths(const std::vector<std::string>& paths,
                                     const std::optional<PathFilter>& filter)
{
    if (!filter)
        return paths;

    auto kept = std::vector<std::string> {};

    for (const auto& path: paths)
        if (filter->matches(path))
            kept.push_back(path);

    return kept;
}
} // namespace diffview
