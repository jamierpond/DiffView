#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
// A trailing newline makes no empty last line; "" is {}.
std::vector<std::string> splitLines(std::string_view text);

// `git ... -z` output.
std::vector<std::string> splitNul(std::string_view text);

std::string trim(std::string_view text);
std::string toLower(std::string_view text);

// Control bytes become '?' so file content can never drive the terminal.
std::string displayable(std::string_view line, int tabWidth = 4);

int codepointCount(std::string_view text);

// Never empty: "" wraps to {""}.
std::vector<std::string> wrap(std::string_view line, int width);

// Stable across runs and platforms, unlike std::hash.
std::uint64_t fnv1a(std::string_view text,
                    std::uint64_t seed = 14695981039346656037ull);
} // namespace diffview
