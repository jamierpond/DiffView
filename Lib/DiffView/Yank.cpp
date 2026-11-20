#include "Yank.h"

#include <eacp/Core/Utils/Base64.h>

#include <algorithm>

namespace diffview
{
namespace
{
constexpr auto osc52Start = std::string_view {"\033]52;c;"};
constexpr auto osc52End = std::string_view {"\a"};

const DiffLine& findLineOfRow(const FileDiff& file, const DiffRow& row)
{
    const auto hunkIndex = static_cast<std::size_t>(row.hunk);
    const auto lineIndex = static_cast<std::size_t>(row.line);
    return file.hunks[hunkIndex].lines[lineIndex];
}
} // namespace

YankedLines collectYankedLines(const FileDiff& file,
                               const std::vector<DiffRow>& rows,
                               int firstRow,
                               int lastRow)
{
    auto yanked = YankedLines {};
    const auto rowCount = static_cast<int>(rows.size());
    const auto from = std::max(std::min(firstRow, lastRow), 0);
    const auto to = std::min(std::max(firstRow, lastRow), rowCount - 1);

    for (auto index = from; index <= to; ++index)
    {
        const auto& row = rows[static_cast<std::size_t>(index)];

        if (row.kind != RowKind::line)
            continue;

        yanked.text += findLineOfRow(file, row).text;
        yanked.text += '\n';
        ++yanked.lineCount;
    }

    return yanked;
}

std::string makeOsc52CopySequence(std::string_view text)
{
    return std::string {osc52Start} + eacp::Base64::encode(text)
           + std::string {osc52End};
}
} // namespace diffview
