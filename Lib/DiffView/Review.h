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
constexpr auto storeDirectoryName = ".diffview";

// The Markdown files are the only store. latest.md is a copy of the newest
// round, not a link: every agent and tool can read a plain file. Names are
// inside the store, except images: comments link them from the main worktree.
constexpr auto draftFileName = "draft.md";
constexpr auto reviewDirectory = "reviews";
constexpr auto latestReviewFileName = "latest.md";
constexpr auto imageDirectory = ".diffview/images";
constexpr auto agentDirectory = "agents";
constexpr auto seenFileName = "seen";
constexpr auto worktreeDirectoryName = "worktrees";

// "2026-10-16-153012": sorts as it reads, so file order is time order.
std::string roundId(std::int64_t epochMs);

std::string roundLabel(const std::string& id);

std::string nextRoundId(const std::vector<std::string>& existing,
                        std::int64_t epochMs);

std::string formatReplyTime(std::int64_t epochMs);

bool isAgentName(const std::string& name);

// The newest version of each id: the draft's over the rounds', later rounds
// over earlier. Hand-written comments get ids, and `renumbered` says the
// draft needs writing back with them.
struct Reviews
{
    std::vector<std::string> rounds;
    std::vector<Annotation> annotations;
    bool renumbered = false;
};

// `reservedUpTo`: ids other worktrees' drafts already hold.
Reviews mergeReviews(const std::vector<ReviewFile>& rounds,
                     const ReviewFile& draft,
                     int reservedUpTo = 0);

std::optional<std::size_t> findPendingReply(const Annotation& comment);

int draftCount(const std::vector<Annotation>& annotations);

int openCount(const std::vector<Annotation>& annotations);

// For `dv monitor`: the round ids (any order) not seen before,
// oldest first.
std::vector<std::string> newRounds(const std::vector<std::string>& seen,
                                   std::vector<std::string> current);

std::vector<std::string> keepRoundsUpTo(std::vector<std::string> ids,
                                        const std::string& after);

struct RoundRoute
{
    int open = 0;
    int replied = 0;

    bool wakes() const { return open > 0 || replied > 0; }
};

// A thread whose owner isn't listening is routed as nobody's.
RoundRoute routeRound(const ReviewFile& round,
                      const std::string& agent,
                      const std::vector<std::string>& listening);

std::string findListeningOwner(const Annotation& comment,
                               const std::vector<std::string>& listening);

// One line per submitted round, for an agent's monitor to wake on.
std::string roundNotice(const std::string& id,
                        const RoundRoute& counts,
                        const std::string& agent,
                        const std::string& worktree,
                        const std::string& path);

std::string replyNotice(const std::string& id,
                        const ReviewFile& round,
                        const std::string& agent,
                        const std::string& worktree,
                        const std::string& path);

ReviewFile makeReplyRound(Annotation thread,
                          const std::string& text,
                          const std::string& time,
                          const std::string& id,
                          const std::string& baseLabel,
                          const std::string& baseSha);

ReviewFile draftFile(const std::vector<Annotation>& annotations,
                     const std::string& baseLabel,
                     const std::string& baseSha);

// An agent resolves comments in the latest.md it was handed; that copy is
// overwritten by the next round, so the change goes into the round's file.
std::optional<ReviewFile> resolutionsFrom(const ReviewFile& latest,
                                          const ReviewFile& round);

struct CommentThreadChange
{
    std::string from = "agent";
    bool named = false;
    bool force = false;
    bool resolve = false;
    std::string text;
    std::string time;
    std::vector<std::string> listening;
};

std::optional<std::string> changeCommentThread(ReviewFile& round,
                                               int id,
                                               const CommentThreadChange& change);

// Withdrawn comments go out once more, so the agent hears, then disappear.
ReviewFile submitDraft(std::vector<Annotation>& annotations,
                       const std::string& id,
                       const std::string& baseLabel,
                       const std::string& baseSha);
} // namespace diffview
