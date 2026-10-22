#include "IntraLine.h"

#include "Text.h"

#include <algorithm>

namespace diffview
{
namespace
{
// Above this share of both lines changed, the pair reads as a rewrite.
constexpr auto wholeLineChangedFraction = 0.8;

bool isContinuationByte(char byte)
{
    return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
}

bool isNotAtCodepointStart(std::string_view text, std::size_t index)
{
    return index < text.size() && isContinuationByte(text[index]);
}

std::size_t findCommonPrefixLength(std::string_view a, std::string_view b)
{
    auto limit = std::min(a.size(), b.size());
    auto length = std::size_t {0};

    while (length < limit && a[length] == b[length])
        ++length;

    while (length > 0
           && (isNotAtCodepointStart(a, length) || isNotAtCodepointStart(b, length)))
        --length;

    return length;
}

std::size_t findCommonSuffixLength(std::string_view a,
                                   std::string_view b,
                                   std::size_t prefixLength)
{
    auto limit = std::min(a.size(), b.size()) - prefixLength;
    auto length = std::size_t {0};

    while (length < limit && a[a.size() - 1 - length] == b[b.size() - 1 - length])
        ++length;

    while (length > 0
           && (isNotAtCodepointStart(a, a.size() - length)
               || isNotAtCodepointStart(b, b.size() - length)))
        --length;

    return length;
}

bool isMostlyChanged(std::size_t changedLength, std::size_t lineLength)
{
    auto changed = static_cast<double>(changedLength);
    auto whole = static_cast<double>(lineLength);

    return lineLength > 0 && changed > whole * wholeLineChangedFraction;
}

std::size_t findRunStart(const Hunk& hunk, std::size_t index, LineKind kind)
{
    while (index > 0 && hunk.lines[index - 1].kind == kind)
        --index;

    return index;
}

std::size_t findRunEnd(const Hunk& hunk, std::size_t index, LineKind kind)
{
    while (index < hunk.lines.size() && hunk.lines[index].kind == kind)
        ++index;

    return index;
}
} // namespace

std::optional<ChangedSpanPair> findChangedSpans(std::string_view removed,
                                                std::string_view added)
{
    if (removed == added)
        return std::nullopt;

    auto prefix = findCommonPrefixLength(removed, added);
    auto suffix = findCommonSuffixLength(removed, added, prefix);

    auto spans = ChangedSpanPair {{prefix, removed.size() - prefix - suffix},
                                  {prefix, added.size() - prefix - suffix}};

    if (isMostlyChanged(spans.removed.length, removed.size())
        && isMostlyChanged(spans.added.length, added.size()))
        return std::nullopt;

    return spans;
}

std::optional<ChangedSpan> findChangedSpanOfLine(const Hunk& hunk,
                                                 std::size_t lineIndex)
{
    if (lineIndex >= hunk.lines.size())
        return std::nullopt;

    auto kind = hunk.lines[lineIndex].kind;

    if (kind == LineKind::context)
        return std::nullopt;

    auto removedStart = std::size_t {0};
    auto addedStart = std::size_t {0};

    if (kind == LineKind::removed)
    {
        removedStart = findRunStart(hunk, lineIndex, LineKind::removed);
        addedStart = findRunEnd(hunk, lineIndex, LineKind::removed);
    }
    else
    {
        addedStart = findRunStart(hunk, lineIndex, LineKind::added);
        if (addedStart == 0 || hunk.lines[addedStart - 1].kind != LineKind::removed)
            return std::nullopt;
        removedStart = findRunStart(hunk, addedStart - 1, LineKind::removed);
    }

    auto removedCount = addedStart - removedStart;
    auto addedCount = findRunEnd(hunk, addedStart, LineKind::added) - addedStart;

    if (removedCount != addedCount)
        return std::nullopt;

    auto offset =
        lineIndex - (kind == LineKind::removed ? removedStart : addedStart);
    auto removedText = displayable(hunk.lines[removedStart + offset].text);
    auto addedText = displayable(hunk.lines[addedStart + offset].text);
    auto spans = findChangedSpans(removedText, addedText);

    if (!spans)
        return std::nullopt;

    if (kind == LineKind::removed)
        return spans->removed;

    return spans->added;
}
} // namespace diffview
