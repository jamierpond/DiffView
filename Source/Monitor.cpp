#include "Monitor.h"
#include "Agents.h"

#include <Review.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

namespace diffview
{
namespace
{
std::vector<std::string> roundFiles(const std::filesystem::path& dir)
{
    auto names = std::vector<std::string> {};
    auto error = std::error_code {};

    // Submit writes atomically, so a name that is there is a whole file.
    if (std::filesystem::is_directory(dir, error))
        for (const auto& entry: std::filesystem::directory_iterator {dir, error})
            if (entry.path().extension() == ".md")
                names.push_back(entry.path().filename().string());

    return names;
}

std::string readFile(const std::filesystem::path& path)
{
    auto in = std::ifstream {path, std::ios::binary};
    auto buffer = std::stringstream {};
    buffer << in.rdbuf();
    return buffer.str();
}
} // namespace

// A one-second poll: the native watcher needs eacp's run loop, and a review
// round is a person pressing a key.
int runMonitor(const std::string& repo, const MonitorOptions& options)
{
    auto root = std::filesystem::absolute(repo);
    auto dir = root / reviewDirectory;
    auto latest = (root / latestReviewFileName).string();
    auto seen = options.after ? keepRoundsUpTo(roundFiles(dir), *options.after)
                              : roundFiles(dir);

    if (!options.agent.empty())
        registerAgent(root.string(), options.agent);

    while (true)
    {
        auto current = roundFiles(dir);
        auto newest = std::max_element(current.begin(), current.end());
        auto woke = false;

        for (const auto& name: newRounds(seen, current))
        {
            auto counts =
                routeRound(fromMarkdown(readFile(dir / name)), options.agent);

            if (!options.agent.empty() && !counts.wakes())
                continue;

            auto id = std::filesystem::path {name}.stem().string();
            auto path = name == *newest ? latest : (dir / name).string();
            auto line = roundNotice(id, counts, options.agent, path);

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
