#include "Monitor.h"

#include <Review.h>

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
int runMonitor(const std::string& repo, bool once)
{
    auto root = std::filesystem::path {repo};
    auto dir = root / reviewDirectory;
    auto latest = (root / latestReviewFileName).string();
    auto seen = roundFiles(dir);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds {1});

        auto current = roundFiles(dir);

        for (const auto& name: newRounds(seen, current))
        {
            auto round = fromMarkdown(readFile(dir / name));
            auto id = std::filesystem::path {name}.stem().string();
            auto line = roundNotice(id, openCount(round.comments), latest);

            std::printf("%s\n", line.c_str());
            std::fflush(stdout);

            if (once)
                return 0;
        }

        seen = std::move(current);
    }
}
} // namespace diffview
