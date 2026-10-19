#include "Review.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <tuple>

namespace diffview
{
std::string roundId(std::int64_t epochMs)
{
    auto seconds = static_cast<std::time_t>(epochMs / 1000);
    auto local = std::tm {};
    localtime_r(&seconds, &local);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H%M%S", &local);
    return buffer;
}

std::string roundLabel(const std::string& id)
{
    if (id.size() < 17)
        return id;

    return id.substr(5, 5) + " " + id.substr(11, 2) + ":" + id.substr(13, 2);
}

std::string formatReplyTime(std::int64_t epochMs)
{
    auto seconds = static_cast<std::time_t>(epochMs / 1000);
    auto local = std::tm {};
    localtime_r(&seconds, &local);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
    return buffer;
}

bool isAgentName(const std::string& name)
{
    return !name.empty() && name.front() != '.' && name.front() != '-'
           && std::all_of(name.begin(),
                          name.end(),
                          [](char c)
                          {
                              return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                                     || (c >= '0' && c <= '9') || c == '-'
                                     || c == '_' || c == '.';
                          });
}

namespace
{
bool isSameReply(const Reply& a, const Reply& b)
{
    return a.author == b.author && a.time == b.time && a.text == b.text;
}

void carryOver(Annotation& newer, const Annotation& older)
{
    auto replies = older.replies;

    for (const auto& reply: newer.replies)
        if (std::none_of(replies.begin(),
                         replies.end(),
                         [&](const Reply& r) { return isSameReply(r, reply); }))
            replies.push_back(reply);

    std::stable_sort(replies.begin(),
                     replies.end(),
                     [](const Reply& a, const Reply& b) { return a.time < b.time; });
    newer.replies = std::move(replies);

    if (newer.owner.empty())
        newer.owner = older.owner;
}

bool hasFresh(const Annotation& comment)
{
    return std::any_of(comment.replies.begin(),
                       comment.replies.end(),
                       [](const Reply& r) { return r.fresh; });
}
} // namespace

Reviews mergeReviews(const std::vector<ReviewFile>& rounds, const ReviewFile& draft)
{
    auto reviews = Reviews {};
    auto byId = std::map<int, Annotation> {};
    auto nextId = 1;

    for (const auto& round: rounds)
        for (const auto& comment: round.comments)
            nextId = std::max(nextId, comment.id + 1);

    for (const auto& comment: draft.comments)
        nextId = std::max(nextId, comment.id + 1);

    for (const auto& round: rounds)
    {
        reviews.rounds.push_back(round.title);

        for (auto comment: round.comments)
        {
            if (comment.id <= 0)
                comment.id = nextId++;

            comment.round = round.title;
            comment.draft = false;

            for (auto& reply: comment.replies)
                reply.fresh = false;

            if (auto older = byId.find(comment.id); older != byId.end())
                carryOver(comment, older->second);

            byId[comment.id] = comment;
        }
    }

    for (auto comment: draft.comments)
    {
        if (comment.id <= 0)
        {
            comment.id = nextId++;
            reviews.renumbered = true;
        }

        auto sent = byId.find(comment.id);
        comment.round = sent != byId.end() ? sent->second.round : std::string {};
        comment.draft = true;

        for (auto& reply: comment.replies)
            reply.fresh = true;

        if (sent != byId.end())
            carryOver(comment, sent->second);

        byId[comment.id] = comment;
    }

    for (const auto& [id, comment]: byId)
        if (!(comment.withdrawn && !comment.draft))
            reviews.annotations.push_back(comment);

    return reviews;
}

std::optional<std::size_t> findPendingReply(const Annotation& comment)
{
    for (std::size_t i = 0; i < comment.replies.size(); ++i)
        if (comment.replies[i].fresh && comment.replies[i].author == reviewerName)
            return i;

    return std::nullopt;
}

int draftCount(const std::vector<Annotation>& annotations)
{
    return static_cast<int>(std::count_if(annotations.begin(),
                                          annotations.end(),
                                          [](const Annotation& a)
                                          { return a.draft; }));
}

int openCount(const std::vector<Annotation>& annotations)
{
    return static_cast<int>(std::count_if(annotations.begin(),
                                          annotations.end(),
                                          [](const Annotation& a)
                                          { return !a.resolved && !a.withdrawn; }));
}

std::vector<std::string> newRounds(const std::vector<std::string>& seen,
                                   std::vector<std::string> current)
{
    std::erase_if(
        current,
        [&](const std::string& name)
        { return std::find(seen.begin(), seen.end(), name) != seen.end(); });
    std::sort(current.begin(), current.end());
    return current;
}

std::vector<std::string> keepRoundsUpTo(std::vector<std::string> names,
                                        const std::string& after)
{
    std::erase_if(names,
                  [&](const std::string& name)
                  { return name.substr(0, name.rfind(".md")) > after; });
    return names;
}

RoundRoute routeRound(const ReviewFile& round, const std::string& agent)
{
    auto counts = RoundRoute {};

    for (const auto& comment: round.comments)
    {
        auto owner = findOwner(comment);
        auto named = false;

        for (const auto& reply: comment.replies)
            if (reply.fresh)
                for (const auto& name: findMentions(reply.text))
                    named = named || name == agent;

        auto forMe = agent.empty() || owner.empty() || owner == agent || named;

        if (forMe && !comment.resolved && !comment.withdrawn)
            ++counts.open;

        if (forMe && hasFresh(comment))
            ++counts.replied;
    }

    return counts;
}

std::string roundNotice(const std::string& id,
                        const RoundRoute& counts,
                        const std::string& agent,
                        const std::string& path)
{
    auto line = "dv: review round " + id + " submitted";

    if (!agent.empty())
        line += " · for @" + agent;

    line += " · " + std::to_string(counts.open) + " open";

    if (counts.replied > 0)
        line += " · " + std::to_string(counts.replied) + " with new replies";

    return line + " · read " + path;
}

ReviewFile draftFile(const std::vector<Annotation>& annotations,
                     const std::string& baseLabel,
                     const std::string& baseSha)
{
    auto file = ReviewFile {"draft", baseLabel, baseSha, {}};

    for (auto annotation: annotations)
        if (annotation.draft)
        {
            annotation.updates = annotation.round;
            annotation.owner = findOwner(annotation);
            file.comments.push_back(annotation);
        }

    std::stable_sort(file.comments.begin(),
                     file.comments.end(),
                     [](const Annotation& a, const Annotation& b)
                     {
                         return std::make_tuple(a.path, !a.fileLevel, a.start, a.id)
                                < std::make_tuple(
                                    b.path, !b.fileLevel, b.start, b.id);
                     });

    return file;
}

std::optional<ReviewFile> resolutionsFrom(const ReviewFile& latest,
                                          const ReviewFile& round)
{
    auto updated = round;
    auto changed = false;

    for (auto& comment: updated.comments)
        for (const auto& copy: latest.comments)
            if (copy.id == comment.id
                && (copy.resolved != comment.resolved || copy.owner != comment.owner
                    || copy.replies != comment.replies))
            {
                comment.resolved = copy.resolved;
                comment.owner = copy.owner;
                comment.replies = copy.replies;
                changed = true;
            }

    return changed ? std::optional<ReviewFile> {updated} : std::nullopt;
}

std::optional<std::string>
    changeThread(ReviewFile& round, int id, const ThreadChange& change)
{
    auto found = std::find_if(round.comments.begin(),
                              round.comments.end(),
                              [&](const Annotation& a) { return a.id == id; });

    if (found == round.comments.end())
        return "no comment #" + std::to_string(id);

    auto owner = findOwner(*found);

    if (!owner.empty() && owner != change.from && !change.force)
        return "#" + std::to_string(id) + " belongs to @" + owner
               + "; pass --force to answer it anyway";

    if (owner.empty() && change.named)
        found->owner = change.from;
    else if (found->owner.empty())
        found->owner = owner;

    if (!change.text.empty())
        found->replies.push_back({change.from, change.time, change.text, false});

    if (change.resolve)
        found->resolved = true;

    return std::nullopt;
}

ReviewFile submitDraft(std::vector<Annotation>& annotations,
                       const std::string& id,
                       const std::string& baseLabel,
                       const std::string& baseSha)
{
    auto file = draftFile(annotations, baseLabel, baseSha);
    file.title = id;

    for (auto& annotation: annotations)
        if (annotation.draft)
        {
            annotation.draft = false;
            annotation.round = id;
            annotation.owner = findOwner(annotation);

            for (auto& reply: annotation.replies)
                reply.fresh = false;
        }

    std::erase_if(annotations, [](const Annotation& a) { return a.withdrawn; });
    return file;
}
} // namespace diffview
