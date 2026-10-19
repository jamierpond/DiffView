#include "Agents.h"

#include <Review.h>
#include <Store.h>
#include <Text.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <eacp/Core/Utils/Files.h>
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

std::optional<pid_t> readListenerProcessId(const std::filesystem::path& registration)
{
    auto in = std::ifstream {registration};
    auto word = std::string {};
    auto pid = pid_t {0};

    if (!(in >> word >> pid) || word != "pid" || !isAlive(pid))
        return std::nullopt;

    return pid;
}

std::filesystem::path makeRegistrationPath(const std::string& store,
                                           const std::string& name)
{
    auto dir = std::filesystem::path {store} / agentDirectory;
    auto error = std::error_code {};
    std::filesystem::create_directories(dir, error);
    return dir / name;
}

constexpr auto responderMark = std::string_view {"responder"};

void writeRegistration(const std::filesystem::path& path, std::string_view mark)
{
    auto startedMs = static_cast<std::int64_t>(std::time(nullptr)) * 1000;
    std::ofstream {path} << "pid " << getpid() << "\nstarted " << roundId(startedMs)
                         << "\n"
                         << mark;
}
} // namespace

void registerAgent(const std::string& store, const std::string& name)
{
    createStoreDirectory(store);
    auto path = makeRegistrationPath(store, name).string();
    std::snprintf(registered.data(), registered.size(), "%s", path.c_str());
    writeRegistration(path, {});

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

void registerResponder(const std::string& store, const std::string& name)
{
    createStoreDirectory(store);
    writeRegistration(makeRegistrationPath(store, name),
                      std::string {responderMark} + "\n");
}

void unregisterResponder(const std::string& store, const std::string& name)
{
    auto path = std::filesystem::path {store} / agentDirectory / name;

    if (readListenerProcessId(path) == getpid())
        unlink(path.c_str());
}

bool isRunningResponder(const std::string& store, const std::string& name)
{
    auto path = std::filesystem::path {store} / agentDirectory / name;

    if (!readListenerProcessId(path))
        return false;

    auto lines = splitLines(eacp::Files::readFile(eacp::FilePath {path.string()}));
    return std::find(lines.begin(), lines.end(), responderMark) != lines.end();
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
        if (readListenerProcessId(entry.path()))
            names.push_back(entry.path().filename().string());
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::optional<int> findListenerProcessId(const std::string& store,
                                         const std::string& name)
{
    auto pid =
        readListenerProcessId(std::filesystem::path {store} / agentDirectory / name);

    if (!pid)
        return std::nullopt;

    return static_cast<int>(*pid);
}

std::vector<AgentActivity> findStatusActivities(const eacp::FilePath& statuses,
                                                std::time_t now)
{
    auto activities = std::vector<AgentActivity> {};

    for (const auto& path: eacp::Files::listFiles(statuses))
    {
        auto status = parseAgentStatus(eacp::Files::readFile(path));

        if (isAgentStatusCurrent(status, now))
            activities.push_back({path.fileName(), status.thread, status.doing});
    }

    return activities;
}
} // namespace diffview
