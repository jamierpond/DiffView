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
        byId[comment.id] = comment;
    }

    for (const auto& [id, comment]: byId)
        if (!(comment.withdrawn && !comment.draft))
            reviews.annotations.push_back(comment);

    return reviews;
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

std::string roundNotice(const std::string& id, int open, const std::string& latest)
{
    return "dv: review round " + id + " submitted · " + std::to_string(open)
           + " open · read " + latest;
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
            if (copy.id == comment.id && copy.resolved != comment.resolved)
            {
                comment.resolved = copy.resolved;
                changed = true;
            }

    return changed ? std::optional<ReviewFile> {updated} : std::nullopt;
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
        }

    std::erase_if(annotations, [](const Annotation& a) { return a.withdrawn; });
    return file;
}
} // namespace diffview
