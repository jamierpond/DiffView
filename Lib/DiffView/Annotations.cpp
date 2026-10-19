#include "Annotations.h"

#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace diffview
{
namespace
{
std::string_view body(const std::string& prefixed)
{
    return prefixed.empty() ? std::string_view {}
                            : std::string_view {prefixed}.substr(1);
}

const DiffLine& lineAt(const FileDiff& file, const FlatLine& at)
{
    return file.hunks[static_cast<std::size_t>(at.hunk)]
        .lines[static_cast<std::size_t>(at.line)];
}

int lineNumber(const DiffLine& line, Side side)
{
    return side == Side::newSide ? line.newLine : line.oldLine;
}
} // namespace

namespace
{
std::optional<LineRange> rangeByNumbers(const Annotation& annotation,
                                        const FileDiff& file)
{
    auto flat = flatLines(file);
    auto range = std::optional<LineRange> {};
    auto end = std::max(annotation.start, annotation.end);

    for (std::size_t i = 0; i < flat.size(); ++i)
    {
        auto number = lineNumber(lineAt(file, flat[i]), annotation.side);

        if (number < annotation.start || number > end)
            continue;

        if (!range)
            range = LineRange {static_cast<int>(i), static_cast<int>(i)};

        range->last = static_cast<int>(i);
    }

    return range;
}
} // namespace

std::vector<FlatLine> flatLines(const FileDiff& file)
{
    auto flat = std::vector<FlatLine> {};

    for (std::size_t h = 0; h < file.hunks.size(); ++h)
        for (std::size_t l = 0; l < file.hunks[h].lines.size(); ++l)
            flat.push_back({static_cast<int>(h), static_cast<int>(l)});

    return flat;
}

Annotation
    makeAnnotation(const FileDiff& file, LineRange range, int id, std::string text)
{
    auto flat = flatLines(file);
    auto annotation = Annotation {};
    annotation.id = id;
    annotation.path = file.path;
    annotation.text = std::move(text);

    if (flat.empty())
        return annotation;

    auto last = static_cast<int>(flat.size()) - 1;
    range.first = std::clamp(range.first, 0, last);
    range.last = std::clamp(range.last, range.first, last);

    auto hunk = flat[static_cast<std::size_t>(range.first)].hunk;

    for (auto i = range.first; i <= range.last; ++i)
        annotation.lines.push_back(
            lineAt(file, flat[static_cast<std::size_t>(i)]).prefixed());

    for (auto i = std::max(0, range.first - contextLines); i < range.first; ++i)
        if (flat[static_cast<std::size_t>(i)].hunk == hunk)
            annotation.before.push_back(
                lineAt(file, flat[static_cast<std::size_t>(i)]).prefixed());

    auto lastHunk = flat[static_cast<std::size_t>(range.last)].hunk;

    for (auto i = range.last + 1; i <= std::min(last, range.last + contextLines);
         ++i)
        if (flat[static_cast<std::size_t>(i)].hunk == lastHunk)
            annotation.after.push_back(
                lineAt(file, flat[static_cast<std::size_t>(i)]).prefixed());

    auto hasNewSide = false;

    for (auto i = range.first; i <= range.last; ++i)
        if (lineAt(file, flat[static_cast<std::size_t>(i)]).newLine > 0)
            hasNewSide = true;

    annotation.side = hasNewSide ? Side::newSide : Side::oldSide;
    annotation.unchangedFile = file.status == FileStatus::unchanged;

    for (auto i = range.first; i <= range.last; ++i)
    {
        auto number = lineNumber(lineAt(file, flat[static_cast<std::size_t>(i)]),
                                 annotation.side);

        if (number <= 0)
            continue;

        if (annotation.start == 0)
            annotation.start = number;

        annotation.end = number;
    }

    return annotation;
}

std::optional<LineRange> findAnchor(const Annotation& annotation,
                                    const FileDiff& file)
{
    if (annotation.lines.empty() || file.path != annotation.path)
        return std::nullopt;

    auto flat = flatLines(file);
    auto count = static_cast<int>(flat.size());
    auto wanted = static_cast<int>(annotation.lines.size());

    auto textAt = [&](int i) -> const std::string&
    { return lineAt(file, flat[static_cast<std::size_t>(i)]).text; };

    auto candidates = std::vector<LineRange> {};

    for (auto p = 0; p + wanted <= count; ++p)
    {
        auto matches = true;

        for (auto i = 0; i < wanted && matches; ++i)
            matches =
                textAt(p + i) == body(annotation.lines[static_cast<std::size_t>(i)]);

        if (matches)
            candidates.push_back({p, p + wanted - 1});
    }

    // Lines inside the range changed: hold on by its two ends.
    if (candidates.empty() && wanted >= 2)
    {
        auto firstText = body(annotation.lines.front());
        auto lastText = body(annotation.lines.back());
        auto reach = wanted * 2 + 10;

        for (auto p = 0; p < count; ++p)
        {
            if (textAt(p) != firstText)
                continue;

            for (auto q = p + 1; q < std::min(count, p + reach); ++q)
                if (textAt(q) == lastText)
                {
                    candidates.push_back({p, q});
                    break;
                }
        }
    }

    if (candidates.empty())
        return std::nullopt;

    auto score = [&](const LineRange& range)
    {
        auto context = 0;

        for (std::size_t k = 0; k < annotation.before.size(); ++k)
        {
            auto i = range.first - 1 - static_cast<int>(k);

            if (i >= 0
                && textAt(i)
                       == body(annotation.before[annotation.before.size() - 1 - k]))
                ++context;
        }

        for (std::size_t k = 0; k < annotation.after.size(); ++k)
        {
            auto i = range.last + 1 + static_cast<int>(k);

            if (i < count && textAt(i) == body(annotation.after[k]))
                ++context;
        }

        auto kinds = 0;

        for (auto i = range.first; i <= range.last && i - range.first < wanted; ++i)
            if (lineAt(file, flat[static_cast<std::size_t>(i)]).prefixed()
                == annotation.lines[static_cast<std::size_t>(i - range.first)])
                ++kinds;

        auto distance = 0;

        for (auto i = range.first; i <= range.last; ++i)
        {
            auto number = lineNumber(lineAt(file, flat[static_cast<std::size_t>(i)]),
                                     annotation.side);

            if (number > 0)
            {
                distance = std::abs(number - annotation.start);
                break;
            }
        }

        return std::make_tuple(context, kinds, -distance);
    };

    return *std::max_element(candidates.begin(),
                             candidates.end(),
                             [&](const LineRange& a, const LineRange& b)
                             { return score(a) < score(b); });
}

Annotation makeFileAnnotation(const std::string& path, int id, std::string text)
{
    auto annotation = Annotation {};
    annotation.id = id;
    annotation.path = path;
    annotation.fileLevel = true;
    annotation.text = std::move(text);
    return annotation;
}

bool reanchor(Annotation& annotation, const FileDiff& file)
{
    if (annotation.fileLevel)
    {
        auto wasStale = annotation.stale;
        annotation.stale = false;
        return wasStale;
    }

    auto range = annotation.lines.empty() ? rangeByNumbers(annotation, file)
                                          : findAnchor(annotation, file);

    if (!range)
    {
        auto wasStale = annotation.stale;
        annotation.stale = true;
        return !wasStale;
    }

    auto moved = makeAnnotation(file, *range, annotation.id, annotation.text);
    auto hasMoved =
        annotation.side != moved.side || annotation.start != moved.start
        || annotation.end != moved.end || annotation.before != moved.before
        || annotation.lines != moved.lines || annotation.after != moved.after
        || annotation.unchangedFile != moved.unchangedFile || annotation.stale;

    annotation.side = moved.side;
    annotation.start = moved.start;
    annotation.end = moved.end;
    annotation.before = moved.before;
    annotation.lines = moved.lines;
    annotation.after = moved.after;
    annotation.unchangedFile = moved.unchangedFile;
    annotation.stale = false;

    return hasMoved;
}

bool reanchorAll(std::vector<Annotation>& annotations,
                 const std::vector<FileDiff>& files,
                 const std::function<const FileDiff*(const std::string&)>& unchanged)
{
    auto changed = false;

    for (auto& annotation: annotations)
    {
        if (annotation.path.empty())
            continue;

        auto file = std::find_if(files.begin(),
                                 files.end(),
                                 [&](const FileDiff& f)
                                 { return f.path == annotation.path; });

        const auto* found =
            file != files.end() ? &*file : unchanged(annotation.path);

        if (found == nullptr)
        {
            changed = changed || !annotation.stale;
            annotation.stale = true;
            continue;
        }

        changed = reanchor(annotation, *found) || changed;
    }

    return changed;
}

std::vector<std::string> imagesIn(const std::string& text)
{
    auto images = std::vector<std::string> {};
    auto marker = std::string {"](.diffview/images/"};

    for (auto at = text.find(marker); at != std::string::npos;
         at = text.find(marker, at + 1))
    {
        auto start = at + 2;
        auto close = text.find(')', start);

        if (close == std::string::npos)
            break;

        auto path = text.substr(start, close - start);

        if (std::find(images.begin(), images.end(), path) == images.end())
            images.push_back(path);
    }

    return images;
}

int nextAnnotationId(const std::vector<Annotation>& annotations)
{
    auto id = 0;

    for (const auto& annotation: annotations)
        id = std::max(id, annotation.id);

    return id + 1;
}

std::string lineLabel(const Annotation& annotation)
{
    if (annotation.path.empty())
        return "general";

    if (annotation.fileLevel)
        return "file";

    auto label = std::to_string(annotation.start);

    if (annotation.end != annotation.start)
        label += "-" + std::to_string(annotation.end);

    if (annotation.side == Side::oldSide)
        label += " (old)";

    return label;
}
std::vector<std::string> findMentions(const std::string& text)
{
    auto names = std::vector<std::string> {};

    auto isNameChar = [](char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
               || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    };

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '@' || (i > 0 && isNameChar(text[i - 1])))
            continue;

        auto end = i + 1;

        while (end < text.size() && isNameChar(text[end]))
            ++end;

        auto name = text.substr(i + 1, end - i - 1);

        while (!name.empty() && (name.back() == '.' || name.back() == '-'))
            name.pop_back();

        if (!name.empty())
            names.push_back(name);

        i = end - 1;
    }

    return names;
}

std::string findOwner(const Annotation& annotation)
{
    if (!annotation.owner.empty())
        return annotation.owner;

    auto names = findMentions(annotation.text);
    return names.empty() ? std::string {} : names.front();
}
} // namespace diffview
