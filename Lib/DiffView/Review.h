#pragma once

#include "Annotations.h"
#include "ReviewMarkdown.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
constexpr auto annotationDirectory = ".diffview";

// The Markdown files are the only store. latest.md is a copy of the newest
// round, not a link: every agent and tool can read a plain file.
constexpr auto draftFileName = ".diffview/draft.md";
constexpr auto reviewDirectory = ".diffview/reviews";
constexpr auto latestReviewFileName = ".diffview/latest.md";
constexpr auto imageDirectory = ".diffview/images";

// "2026-10-16-153012": sorts as it reads, so file order is time order.
std::string roundId(std::int64_t epochMs);

std::string roundLabel(const std::string& id);

// The newest version of each id: the draft's over the rounds', later rounds
// over earlier. Hand-written comments get ids, and `renumbered` says the
// draft needs writing back with them.
struct Reviews
{
    std::vector<std::string> rounds;
    std::vector<Annotation> annotations;
    bool renumbered = false;
};

Reviews mergeReviews(const std::vector<ReviewFile>& rounds, const ReviewFile& draft);

int draftCount(const std::vector<Annotation>& annotations);

ReviewFile draftFile(const std::vector<Annotation>& annotations,
                     const std::string& baseLabel,
                     const std::string& baseSha);

// An agent resolves comments in the latest.md it was handed; that copy is
// overwritten by the next round, so the change goes into the round's file.
std::optional<ReviewFile> resolutionsFrom(const ReviewFile& latest,
                                          const ReviewFile& round);

// Withdrawn comments go out once more, so the agent hears, then disappear.
ReviewFile submitDraft(std::vector<Annotation>& annotations,
                       const std::string& id,
                       const std::string& baseLabel,
                       const std::string& baseSha);
} // namespace diffview
