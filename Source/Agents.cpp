#include "Agents.h"

#include <Review.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace diffview
{
namespace
{
auto registered = std::array<char, 4096> {};

bool isAlive(pid_t pid)
{
    constexpr auto checkOnly = 0;
    return pid > 0 && (kill(pid, checkOnly) == 0 || errno == EPERM);
}
} // namespace

void registerAgent(const std::string& repo, const std::string& name)
{
    auto dir = std::filesystem::path {repo} / agentDirectory;
    auto error = std::error_code {};
    std::filesystem::create_directories(dir, error);

    auto own = std::filesystem::path {repo} / annotationDirectory / ".gitignore";

    if (!std::filesystem::exists(own))
        std::ofstream {own} << "*\n";

    auto path = (dir / name).string();
    std::snprintf(registered.data(), registered.size(), "%s", path.c_str());

    std::ofstream {path} << "pid " << getpid() << "\nstarted "
                         << roundId(static_cast<std::int64_t>(std::time(nullptr))
                                    * 1000)
                         << "\n";

    for (auto signal: {SIGINT, SIGTERM, SIGHUP, SIGPIPE})
        std::signal(signal,
                    [](int caught)
                    {
                        if (registered[0] != 0)
                            unlink(registered.data());

                        _exit(128 + caught);
                    });
}

void unregisterAgent()
{
    if (registered[0] != 0)
        unlink(registered.data());

    registered[0] = 0;
}

std::vector<std::string> findListeningAgents(const std::string& repo)
{
    auto names = std::vector<std::string> {};
    auto dir = std::filesystem::path {repo} / agentDirectory;
    auto error = std::error_code {};

    if (!std::filesystem::is_directory(dir, error))
        return names;

    for (const auto& entry: std::filesystem::directory_iterator {dir, error})
    {
        auto in = std::ifstream {entry.path()};
        auto word = std::string {};
        auto pid = pid_t {0};

        if (in >> word >> pid && word == "pid" && isAlive(pid))
            names.push_back(entry.path().filename().string());
    }

    std::sort(names.begin(), names.end());
    return names;
}
} // namespace diffview
