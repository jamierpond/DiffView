#include "Monitor.h"
#include "Agents.h"

#include <Review.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace diffview
{
// A one-second poll: the native watcher needs eacp's run loop, and a review
// round is a person pressing a key.
int runMonitor(const StoreLayout& store, const MonitorOptions& options)
{
    const auto& root = store.directory;
    auto seen = listRoundIdsInOrder(root);

    if (options.after)
        seen = keepRoundsUpTo(seen, *options.after);

    if (!options.agent.empty())
        registerAgent(root.str(), options.agent, options.agentPid, options.inbox);

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

            auto line = describeRoundForAgent(
                store, store.worktree.str(), id, round, counts, options.agent);
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
            woke = true;
        }

        seen = std::move(current);

        if (woke && options.once)
        {
            markAgentBusy();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::seconds {1});
    }
}
} // namespace diffview
