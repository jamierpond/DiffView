#pragma once

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace diffview
{
// The tree's file filter: an ECMAScript regex searched in the repo-relative
// path, case-insensitive unless the pattern has an uppercase letter; a
// leading ! keeps the paths that don't match.
struct PathFilter
{
    std::string pattern;
    std::regex expression;
    bool inverted = false;

    bool matches(const std::string& path) const;
};

// nullopt for an empty pattern, a bare !, or an invalid regex.
std::optional<PathFilter> parsePathFilter(const std::string& pattern);

std::vector<std::string> filterPaths(const std::vector<std::string>& paths,
                                     const std::optional<PathFilter>& filter);
} // namespace diffview
