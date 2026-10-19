#pragma once

#include <Review.h>

#include <eacp/Core/Utils/FilePath.h>

#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
std::vector<std::string> listRoundIdsInOrder(const eacp::FilePath& repo);

std::vector<ReviewFile> readRounds(const eacp::FilePath& repo);

ReviewFile readReview(const eacp::FilePath& path);

void writeText(const eacp::FilePath& path, std::string_view text);
} // namespace diffview
