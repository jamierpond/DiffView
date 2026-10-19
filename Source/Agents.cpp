#include "Agents.h"

#include <Review.h>
#include <Store.h>

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

// An agent between `dv monitor --once` runs is still working its threads.
constexpr auto listeningGraceSeconds = std::time_t {600};

bool isAlive(pid_t pid)
{
    constexpr auto checkOnly = 0;
    return pid > 0 && (kill(pid, checkOnly) == 0 || errno == EPERM);
}

bool isListening(const std::filesystem::path& registration)
{
    auto in = std::ifstream {registration};
    auto word = std::string {};
    auto pid = pid_t {0};

    if (!(in >> word >> pid) || word != "pid")
        return false;

    if (isAlive(pid))
        return true;

    auto stopped = std::time_t {0};

    if (!(in >> word >> stopped) || word != "stopped")
        return false;

    return std::time(nullptr) - stopped < listeningGraceSeconds;
}
} // namespace

void registerAgent(const std::string& store, const std::string& name)
{
    createStoreDirectory(store);

    auto dir = std::filesystem::path {store} / agentDirectory;
    auto error = std::error_code {};
    std::filesystem::create_directories(dir, error);

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

void markAgentStopped()
{
    if (registered[0] == 0)
        return;

    std::ofstream {registered.data()} << "pid 0\nstopped " << std::time(nullptr)
                                      << "\n";
    registered[0] = 0;
}

std::vector<std::string> findListeningAgents(const std::string& store)
{
    auto names = std::vector<std::string> {};
    auto dir = std::filesystem::path {store} / agentDirectory;
    auto error = std::error_code {};

    if (!std::filesystem::is_directory(dir, error))
        return names;

    for (const auto& entry: std::filesystem::directory_iterator {dir, error})
    {
        if (isListening(entry.path()))
            names.push_back(entry.path().filename().string());
    }

    std::sort(names.begin(), names.end());
    return names;
}
} // namespace diffview
