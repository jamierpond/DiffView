#include "Monitor.h"
#include "Agents.h"
#include "Store.h"

#include <Review.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace diffview
{
namespace
{
std::string describeRound(const eacp::FilePath& root,
                          const std::string& id,
                          const ReviewFile& round,
                          const RoundRoute& counts,
                          const std::string& agent)
{
    auto own = (root / reviewDirectory / (id + ".md")).str();

    if (round.reply)
        return replyNotice(id, round, agent, own);

    auto latest = root / latestReviewFileName;

    if (readReviewOrEmpty(latest).title == id)
        return roundNotice(id, counts, agent, latest.str());

    return roundNotice(id, counts, agent, own);
}
} // namespace

// A one-second poll: the native watcher needs eacp's run loop, and a review
// round is a person pressing a key.
int runMonitor(const std::string& repo, const MonitorOptions& options)
{
    auto root = eacp::FilePath {std::filesystem::absolute(repo)};
    auto seen = listRoundIdsInOrder(root);

    if (options.after)
        seen = keepRoundsUpTo(seen, *options.after);

    if (!options.agent.empty())
        registerAgent(root.str(), options.agent);

    while (true)
    {
        auto current = listRoundIdsInOrder(root);
        auto woke = false;

        for (const auto& id: newRounds(seen, current))
        {
            auto round = readReviewOrEmpty(root / reviewDirectory / (id + ".md"));
            auto counts = routeRound(round, options.agent);

            if (!options.agent.empty() && !counts.wakes())
                continue;

            auto line = describeRound(root, id, round, counts, options.agent);
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
            woke = true;
        }

        seen = std::move(current);

        if (woke && options.once)
        {
            unregisterAgent();
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::seconds {1});
    }
}
} // namespace diffview
