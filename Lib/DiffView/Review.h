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

// One file per waiting `dv monitor --as <name>`: "pid <n>" and when it began.
constexpr auto agentDirectory = ".diffview/agents";

// "2026-10-16-153012": sorts as it reads, so file order is time order.
std::string roundId(std::int64_t epochMs);

std::string roundLabel(const std::string& id);

// A reply's time: "2026-10-18 14:02".
std::string replyTime(std::int64_t epochMs);

// Agent names go in headings and file names: letters, digits, - _ .
bool isAgentName(const std::string& name);

// The newest version of each id: the draft's over the rounds', later rounds
// over earlier. A thread's replies are gathered from every version, so a
// reply written to an older copy isn't lost; only the draft's own are fresh.
// An owner, once set, stays unless a newer version names another.
// Hand-written comments get ids, and `renumbered` says the draft needs
// writing back with them.
struct Reviews
{
    std::vector<std::string> rounds;
    std::vector<Annotation> annotations;
    bool renumbered = false;
};

Reviews mergeReviews(const std::vector<ReviewFile>& rounds, const ReviewFile& draft);

int draftCount(const std::vector<Annotation>& annotations);

int openCount(const std::vector<Annotation>& annotations);

// For `dv monitor`: the round files (names, any order) not seen before,
// oldest first.
std::vector<std::string> newRounds(const std::vector<std::string>& seen,
                                   std::vector<std::string> current);

// The round files no newer than round `after` (`--after`): already handled.
std::vector<std::string> roundsUpTo(std::vector<std::string> names,
                                    const std::string& after);

// What a round holds for one agent. With no name, everything: open comments
// and threads with new replies from the reviewer. With a name: open comments
// it owns or nobody does, or whose new replies name it; threads it owns (or
// nobody does) with new replies. Unowned ones are everyone's until claimed.
struct RoundFor
{
    int open = 0;
    int replied = 0;

    bool wakes() const { return open > 0 || replied > 0; }
};

RoundFor roundFor(const ReviewFile& round, const std::string& agent);

// One line per submitted round, for an agent's monitor to wake on.
std::string roundNotice(const std::string& id,
                        const RoundFor& counts,
                        const std::string& agent,
                        const std::string& path);

ReviewFile draftFile(const std::vector<Annotation>& annotations,
                     const std::string& baseLabel,
                     const std::string& baseSha);

// An agent resolves comments in the latest.md it was handed; that copy is
// overwritten by the next round, so the change goes into the round's file.
std::optional<ReviewFile> resolutionsFrom(const ReviewFile& latest,
                                          const ReviewFile& round);

// `dv reply` and `dv resolve` on comment #id of the round holding its newest
// version. An unowned thread is claimed by a named agent; one owned by
// another agent is refused unless forced. Returns the error, if any.
struct ThreadChange
{
    std::string from = "agent";
    bool named = false;
    bool force = false;
    bool resolve = false;
    std::string text;
    std::string time;
};

std::optional<std::string>
    changeThread(ReviewFile& round, int id, const ThreadChange& change);

// Withdrawn comments go out once more, so the agent hears, then disappear.
ReviewFile submitDraft(std::vector<Annotation>& annotations,
                       const std::string& id,
                       const std::string& baseLabel,
                       const std::string& baseSha);
} // namespace diffview
