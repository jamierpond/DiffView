#include "Store.h"

#include <eacp/Core/Utils/Files.h>

#include <algorithm>

namespace diffview
{
std::vector<std::string> listRoundIdsInOrder(const eacp::FilePath& repo)
{
    auto ids = std::vector<std::string> {};

    // Rounds are written atomically, so a name that is there is a whole file.
    for (const auto& path:
         eacp::Files::listFiles(repo / reviewDirectory, {.pattern = "*.md"}))
    {
        ids.push_back(path.stem());
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<ReviewFile> readRounds(const eacp::FilePath& repo)
{
    auto rounds = std::vector<ReviewFile> {};

    for (const auto& id: listRoundIdsInOrder(repo))
    {
        rounds.push_back(readReviewOrEmpty(repo / reviewDirectory / (id + ".md")));
        rounds.back().title = id;
    }

    return rounds;
}

ReviewFile readReviewOrEmpty(const eacp::FilePath& path)
{
    return fromMarkdown(eacp::Files::readFile(path));
}

void writeText(const eacp::FilePath& path, std::string_view text)
{
    auto bytes =
        std::span {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    eacp::Files::writeFileAtomically(path, bytes);
}
} // namespace diffview
