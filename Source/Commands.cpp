#include "Agents.h"
#include "Commands.h"
#include "Viewer.h"

#include <Review.h>
#include <Text.h>

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>

namespace diffview
{
namespace
{
bool hasComment(const ReviewFile& file, int id)
{
    return std::any_of(file.comments.begin(),
                       file.comments.end(),
                       [&](const Annotation& a) { return a.id == id; });
}

int fail(const std::string& message)
{
    std::fprintf(stderr, "dv: %s\n", message.c_str());
    return 1;
}

std::string describeMissing(int id)
{
    return "no comment #" + std::to_string(id)
           + " in a submitted round (dv comments lists them)";
}

int changeAndWrite(const ThreadOptions& options, bool resolve)
{
    const auto& root = options.store.directory;

    if (!eacp::Files::isDirectory(root / reviewDirectory))
        return fail("no review rounds in " + root.str()
                    + " yet (nothing has been submitted)");

    auto lock = StoreLock {root};
    auto rounds = readRounds(options.store);
    auto at =
        std::find_if(rounds.rbegin(),
                     rounds.rend(),
                     [&](const ReviewFile& r) { return hasComment(r, options.id); });

    if (at == rounds.rend())
    {
        if (hasComment(
                readReviewOrEmpty(options.store.worktreeDirectory / draftFileName),
                options.id))
            return fail("#" + std::to_string(options.id)
                        + " is still in the reviewer's draft, not sent to you");

        return fail(describeMissing(options.id));
    }

    auto& round = *at;
    auto latestPath = root / latestReviewFileName;
    auto latest = readReviewOrEmpty(latestPath);
    auto isNewest = latest.title == round.title;

    if (isNewest)
    {
        if (auto synced = resolutionsFrom(latest, round))
            round = std::move(*synced);
    }

    auto change = ThreadChange {};
    change.from = options.agent.empty() ? "agent" : options.agent;
    change.named = !options.agent.empty();
    change.force = options.force;
    change.resolve = resolve;
    change.text = options.text;
    change.time = formatReplyTime(nowMs());
    change.listening = findListeningAgents(root.str());

    if (auto error = changeThread(round, options.id, change))
        return fail(*error);

    auto text = toMarkdown(round);
    auto path = root / reviewDirectory / (round.title + ".md");
    writeText(path, text);

    if (isNewest)
        writeText(latestPath, text);

    std::printf("dv: %s #%d · %s\n",
                resolve ? "resolved" : "replied to",
                options.id,
                path.c_str());
    return 0;
}

std::string formatPlace(const Annotation& comment)
{
    if (comment.path.empty())
        return "General";

    if (comment.fileLevel)
        return comment.path + " (whole file)";

    return comment.path + ":" + lineLabel(comment);
}

std::string getOtherWorktreeName(const StoreLayout& store, const Annotation& comment)
{
    if (comment.worktree.empty()
        || isSamePath(comment.worktree, store.worktree.str()))
        return {};

    return getWorktreeShortName(comment.worktree);
}

std::string formatLocation(const StoreLayout& store, const Annotation& comment)
{
    auto worktree = getOtherWorktreeName(store, comment);

    if (worktree.empty())
        return formatPlace(comment);

    return "[" + worktree + "] " + formatPlace(comment);
}

std::string pad(const std::string& text, int width)
{
    auto shown = codepointCount(text);
    return shown < width
               ? text + std::string(static_cast<std::size_t>(width - shown), ' ')
               : text;
}

std::optional<Annotation> find(const std::vector<Annotation>& comments, int id)
{
    for (const auto& comment: comments)
    {
        if (comment.id == id)
            return comment;
    }

    return std::nullopt;
}
} // namespace

int runReply(const ThreadOptions& options)
{
    if (trim(options.text).empty())
        return fail("nothing to say: give the text, -m <text>, or pipe it in");

    return changeAndWrite(options, false);
}

int runResolve(const ThreadOptions& options)
{
    return changeAndWrite(options, true);
}

int runComments(const ListOptions& options)
{
    auto comments = mergeReviews(readRounds(options.store), {}).annotations;
    auto rows = std::vector<std::array<std::string, 5>> {};
    auto listening = findListeningAgents(options.store.directory.str());

    for (const auto& comment: comments)
    {
        auto owner = findOwner(comment);

        if (options.openOnly && comment.resolved)
            continue;

        auto listeningOwner = findListeningOwner(comment, listening);

        if (!options.agent.empty() && !listeningOwner.empty()
            && listeningOwner != options.agent)
            continue;

        auto lines = splitLines(comment.text);
        auto pieces = wrap(lines.empty() ? std::string {} : lines.front(), 72);
        auto about = trim(pieces.front())
                     + (pieces.size() > 1 || lines.size() > 1 ? "…" : "");

        if (!comment.replies.empty())
        {
            about += " · " + std::to_string(comment.replies.size())
                     + (comment.replies.size() == 1 ? " reply" : " replies")
                     + ", last " + comment.replies.back().author;
        }

        rows.push_back({"#" + std::to_string(comment.id),
                        comment.resolved ? "resolved" : "open",
                        owner.empty() ? "-" : "@" + owner,
                        formatLocation(options.store, comment),
                        about});
    }

    auto widths = std::array<int, 4> {};

    for (const auto& row: rows)
    {
        for (std::size_t k = 0; k < widths.size(); ++k)
            widths[k] = std::max(widths[k], codepointCount(row[k]));
    }

    for (const auto& row: rows)
    {
        auto line = std::string {};

        for (std::size_t k = 0; k < widths.size(); ++k)
            line += pad(row[k], widths[k]) + "  ";

        std::printf("%s\n", (line + row[4]).c_str());
    }

    return 0;
}

int runShow(const StoreLayout& store, int id)
{
    auto comments = mergeReviews(readRounds(store), {}).annotations;
    auto comment = find(comments, id);

    if (!comment)
        return fail(describeMissing(id));

    auto shown = *comment;
    shown.owner = findOwner(shown);
    shown.updates.clear();
    std::printf("%s", printComment(shown).c_str());
    auto worktree = getOtherWorktreeName(store, *comment);

    if (worktree.empty())
        std::printf("(round %s)\n", comment->round.c_str());
    else
        std::printf("(round %s, reviewed in the worktree %s at %s)\n",
                    comment->round.c_str(),
                    worktree.c_str(),
                    comment->worktree.c_str());
    return 0;
}
} // namespace diffview
