#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
enum class LineKind
{
    context,
    added,
    removed
};

struct DiffLine
{
    LineKind kind = LineKind::context;

    // 0 on the side the line doesn't exist on.
    int oldLine = 0;
    int newLine = 0;

    std::string text;

    std::string prefixed() const;
};

struct Hunk
{
    int oldStart = 0;
    int oldCount = 0;
    int newStart = 0;
    int newCount = 0;

    std::string section;

    std::vector<DiffLine> lines;

    std::string header() const;
};

enum class FileStatus
{
    modified,
    added,
    deleted,
    untracked,
    unchanged
};

struct FileDiff
{
    std::string path;
    FileStatus status = FileStatus::modified;
    bool binary = false;

    // Why there are no hunks (an untracked file too big to read).
    std::string note;

    std::vector<Hunk> hunks;

    int added = 0;
    int removed = 0;

    std::uint64_t fingerprint = 0;
};

using OptionalFileDiff = std::optional<std::reference_wrapper<const FileDiff>>;

// Expects --no-renames and a/ b/ prefixes (see git::diff).
std::vector<FileDiff> parseDiff(std::string_view text);

// A NUL in the first 8000 bytes means binary, as git decides it.
// A file with no changes, shown whole: every line unchanged.
FileDiff unchangedFile(const std::string& path, std::string_view content);

FileDiff diffForNewFile(const std::string& path, std::string_view content);

std::string unquotePath(std::string_view path);

char statusLetter(FileStatus status);

void finalize(FileDiff& file);
} // namespace diffview
