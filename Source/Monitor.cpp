#include "Monitor.h"
#include "Agents.h"

#include <Review.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace diffview
{
namespace
{
std::string getOtherWorktreeName(const StoreLayout& store, const ReviewFile& round)
{
    auto worktree = round.worktree.empty() ? store.home.str() : round.worktree;

    if (isSamePath(worktree, store.worktree.str()))
        return {};

    return getWorktreeShortName(worktree);
}

std::string describeRound(const StoreLayout& store,
                          const std::string& id,
                          const ReviewFile& round,
                          const RoundRoute& counts,
                          const std::string& agent)
{
    auto own = (store.directory / reviewDirectory / (id + ".md")).str();
    auto worktree = getOtherWorktreeName(store, round);

    if (round.reply)
        return replyNotice(id, round, agent, worktree, own);

    auto latest = store.directory / latestReviewFileName;

    if (readReviewOrEmpty(latest).title == id)
        return roundNotice(id, counts, agent, worktree, latest.str());

    return roundNotice(id, counts, agent, worktree, own);
}
} // namespace

// A one-second poll: the native watcher needs eacp's run loop, and a review
// round is a person pressing a key.
int runMonitor(const StoreLayout& store, const MonitorOptions& options)
{
    const auto& root = store.directory;
    auto seen = listRoundIdsInOrder(root);

    if (options.after)
        seen = keepRoundsUpTo(seen, *options.after);

    if (!options.agent.empty())
        registerAgent(root.str(), options.agent);

    auto listening = std::vector<std::string> {};

    while (true)
    {
        auto current = listRoundIdsInOrder(root);
        auto woke = false;
        auto fresh = newRounds(seen, current);

        if (!fresh.empty())
            listening = findListeningAgents(root.str());

        for (const auto& id: fresh)
        {
            auto round = readReviewOrEmpty(root / reviewDirectory / (id + ".md"));
            auto counts = routeRound(round, options.agent, listening);

            if (!options.agent.empty() && !counts.wakes())
                continue;

            auto line = describeRound(store, id, round, counts, options.agent);
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
            woke = true;
        }

        seen = std::move(current);

        if (woke && options.once)
        {
            markAgentStopped();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::seconds {1});
    }
}
} // namespace diffview
