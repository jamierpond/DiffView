#include "Diff.h"
#include "Text.h"

#include <charconv>

namespace diffview
{
namespace
{
constexpr auto gitHeader = std::string_view {"diff --git "};

void parseRange(std::string_view text, int& start, int& count)
{
    auto comma = text.find(',');
    auto first = text.substr(0, comma);
    std::from_chars(first.data(), first.data() + first.size(), start);
    count = 1;

    if (comma != std::string_view::npos)
    {
        auto second = text.substr(comma + 1);
        std::from_chars(second.data(), second.data() + second.size(), count);
    }
}

bool parseHunkHeader(std::string_view line, Hunk& hunk)
{
    if (!line.starts_with("@@ -"))
        return false;

    auto rest = line.substr(4);
    auto space = rest.find(' ');

    if (space == std::string_view::npos)
        return false;

    parseRange(rest.substr(0, space), hunk.oldStart, hunk.oldCount);
    rest = rest.substr(space + 1);

    if (!rest.starts_with("+"))
        return false;

    rest = rest.substr(1);
    space = rest.find(' ');
    parseRange(rest.substr(0, space), hunk.newStart, hunk.newCount);

    auto close = line.find("@@", 2);

    if (close != std::string_view::npos && close + 2 < line.size())
        hunk.section = trim(line.substr(close + 2));

    return true;
}

// With --no-renames both sides are the same path, so split in the middle:
// splitting on " b/" breaks on names with spaces.
std::string pathFromGitHeader(std::string_view line)
{
    auto rest = line.substr(gitHeader.size());

    if (rest.starts_with("\""))
    {
        auto close = rest.find("\" ");
        auto quoted = rest.substr(
            0, close == std::string_view::npos ? rest.size() : close + 1);
        auto path = unquotePath(quoted);
        return path.starts_with("a/") ? path.substr(2) : path;
    }

    if (rest.size() < 6 || !rest.starts_with("a/"))
        return std::string {rest};

    auto pathLength = (rest.size() - 5) / 2;
    return std::string {rest.substr(2, pathLength)};
}

std::string pathFromMarker(std::string_view line)
{
    auto path = line.substr(4);

    if (!path.empty() && path.back() == '\t')
        path.remove_suffix(1);

    if (path == "/dev/null")
        return {};

    auto unquoted = unquotePath(path);

    if (unquoted.starts_with("a/") || unquoted.starts_with("b/"))
        return unquoted.substr(2);

    return unquoted;
}
} // namespace

std::string DiffLine::prefixed() const
{
    auto marker = kind == LineKind::added     ? '+'
                  : kind == LineKind::removed ? '-'
                                              : ' ';
    return marker + text;
}

std::string Hunk::header() const
{
    auto header = "@@ -" + std::to_string(oldStart) + "," + std::to_string(oldCount)
                  + " +" + std::to_string(newStart) + "," + std::to_string(newCount)
                  + " @@";

    if (!section.empty())
        header += " " + section;

    return header;
}

std::string unquotePath(std::string_view path)
{
    if (path.size() < 2 || path.front() != '"' || path.back() != '"')
        return std::string {path};

    path = path.substr(1, path.size() - 2);
    auto out = std::string {};

    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] != '\\' || i + 1 >= path.size())
        {
            out.push_back(path[i]);
            continue;
        }

        auto c = path[++i];

        switch (c)
        {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 'a':
                out.push_back('\a');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'v':
                out.push_back('\v');
                break;
            default:
                if (c >= '0' && c <= '7' && i + 2 < path.size())
                {
                    auto value = (c - '0') * 64 + (path[i + 1] - '0') * 8
                                 + (path[i + 2] - '0');
                    out.push_back(static_cast<char>(value));
                    i += 2;
                }
                else
                {
                    out.push_back(c);
                }
        }
    }

    return out;
}

char statusLetter(FileStatus status)
{
    switch (status)
    {
        case FileStatus::added:
            return 'A';
        case FileStatus::deleted:
            return 'D';
        case FileStatus::untracked:
            return '?';
        case FileStatus::unchanged:
            return ' ';
        case FileStatus::modified:
            break;
    }

    return 'M';
}

void finalize(FileDiff& file)
{
    file.added = 0;
    file.removed = 0;

    auto hash = fnv1a(file.path);
    hash = fnv1a(std::string(1, statusLetter(file.status)), hash);
    hash = fnv1a(file.note, hash);

    if (file.binary)
        hash = fnv1a("binary", hash);

    for (const auto& hunk: file.hunks)
    {
        hash = fnv1a(hunk.header(), hash);

        for (const auto& line: hunk.lines)
        {
            if (line.kind == LineKind::added)
                ++file.added;
            else if (line.kind == LineKind::removed)
                ++file.removed;

            hash = fnv1a(line.prefixed(), hash);
            hash = fnv1a("\n", hash);
        }
    }

    file.fingerprint = hash;
}

std::vector<FileDiff> parseDiff(std::string_view text)
{
    auto files = std::vector<FileDiff> {};
    auto lines = splitLines(text);

    // Counted down so a removed line reading "-- x" isn't taken for a
    // "--- " header.
    auto oldLeft = 0;
    auto newLeft = 0;
    auto oldLine = 0;
    auto newLine = 0;

    for (const auto& raw: lines)
    {
        auto line = std::string_view {raw};

        if (!files.empty() && !files.back().hunks.empty()
            && (oldLeft > 0 || newLeft > 0))
        {
            auto& hunk = files.back().hunks.back();

            if (line.starts_with("\\"))
                continue;

            auto marker = line.empty() ? ' ' : line.front();
            auto body = line.empty() ? std::string {} : std::string {line.substr(1)};

            if (marker == '+')
            {
                hunk.lines.push_back({LineKind::added, 0, newLine++, body});
                --newLeft;
                continue;
            }

            if (marker == '-')
            {
                hunk.lines.push_back({LineKind::removed, oldLine++, 0, body});
                --oldLeft;
                continue;
            }

            if (marker == ' ')
            {
                hunk.lines.push_back(
                    {LineKind::context, oldLine++, newLine++, body});
                --oldLeft;
                --newLeft;
                continue;
            }

            oldLeft = 0;
            newLeft = 0;
        }

        if (line.starts_with(gitHeader))
        {
            if (!files.empty())
                finalize(files.back());

            auto file = FileDiff {};
            file.path = pathFromGitHeader(line);
            files.push_back(std::move(file));
            continue;
        }

        if (files.empty())
            continue;

        auto& file = files.back();

        if (line.starts_with("@@ -"))
        {
            auto hunk = Hunk {};

            if (!parseHunkHeader(line, hunk))
                continue;

            oldLeft = hunk.oldCount;
            newLeft = hunk.newCount;
            oldLine = hunk.oldStart;
            newLine = hunk.newStart;
            file.hunks.push_back(std::move(hunk));
            continue;
        }

        if (line.starts_with("\\"))
            continue;

        if (!file.hunks.empty())
            continue;

        if (line.starts_with("new file mode"))
            file.status = FileStatus::added;
        else if (line.starts_with("deleted file mode"))
            file.status = FileStatus::deleted;
        else if (line.starts_with("Binary files ")
                 || line.starts_with("GIT binary patch"))
            file.binary = true;
        else if (line.starts_with("+++ "))
        {
            if (auto path = pathFromMarker(line); !path.empty())
                file.path = path;
        }
        else if (line.starts_with("--- "))
        {
            if (auto path = pathFromMarker(line); !path.empty())
                file.path = path;
        }
    }

    if (!files.empty())
        finalize(files.back());

    return files;
}

FileDiff unchangedFile(const std::string& path, std::string_view content)
{
    auto file = diffForNewFile(path, content);
    file.status = FileStatus::unchanged;

    for (auto& hunk: file.hunks)
    {
        hunk.oldStart = hunk.newStart;
        hunk.oldCount = hunk.newCount;

        for (auto& line: hunk.lines)
        {
            line.kind = LineKind::context;
            line.oldLine = line.newLine;
        }
    }

    finalize(file);
    return file;
}

FileDiff diffForNewFile(const std::string& path, std::string_view content)
{
    auto file = FileDiff {};
    file.path = path;
    file.status = FileStatus::untracked;

    if (content.substr(0, 8000).find('\0') != std::string_view::npos)
    {
        file.binary = true;
        finalize(file);
        return file;
    }

    auto lines = splitLines(content);

    if (!lines.empty())
    {
        auto hunk = Hunk {};
        hunk.oldStart = 0;
        hunk.oldCount = 0;
        hunk.newStart = 1;
        hunk.newCount = static_cast<int>(lines.size());

        auto number = 1;

        for (auto& text: lines)
            hunk.lines.push_back({LineKind::added, 0, number++, std::move(text)});

        file.hunks.push_back(std::move(hunk));
    }

    finalize(file);
    return file;
}
} // namespace diffview
