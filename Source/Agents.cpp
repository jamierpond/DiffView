#include "Agents.h"

#include <Listener.h>
#include <Review.h>
#include <Store.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <eacp/Core/Utils/Files.h>
#include <filesystem>
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

ListenerRegistration readRegistration(const std::filesystem::path& path)
{
    return parseListenerRegistration(
        eacp::Files::readFile(eacp::FilePath {path.string()}));
}

std::optional<int> readListenerProcessId(const std::filesystem::path& path)
{
    return findLiveListenerPid(readRegistration(path),
                               [](int pid) { return isAlive(pid); });
}

std::filesystem::path makeRegistrationPath(const std::string& store,
                                           const std::string& name)
{
    auto dir = std::filesystem::path {store} / agentDirectory;
    auto error = std::error_code {};
    std::filesystem::create_directories(dir, error);
    return dir / name;
}

ListenerRegistration makeRegistration()
{
    auto startedMs = static_cast<std::int64_t>(std::time(nullptr)) * 1000;
    auto registration = ListenerRegistration {};
    registration.monitorPid = static_cast<int>(getpid());
    registration.started = roundId(startedMs);
    return registration;
}

void writeRegistration(const std::filesystem::path& path,
                       const ListenerRegistration& registration)
{
    writeText(eacp::FilePath {path.string()},
              formatListenerRegistration(registration));
}

auto registeredAgent = ListenerRegistration {};
} // namespace

void registerAgent(const std::string& store,
                   const std::string& name,
                   std::optional<int> agentPid)
{
    createStoreDirectory(store);
    auto path = makeRegistrationPath(store, name).string();
    std::snprintf(registered.data(), registered.size(), "%s", path.c_str());
    registeredAgent = makeRegistration();
    registeredAgent.agentPid = agentPid.value_or(0);
    writeRegistration(path, registeredAgent);

    for (auto signal: {SIGINT, SIGTERM, SIGHUP, SIGPIPE})
        std::signal(signal,
                    [](int caught)
                    {
                        if (registered[0] != 0)
                            unlink(registered.data());

                        _exit(128 + caught);
                    });
}

void markAgentBusy()
{
    if (registered[0] == 0)
        return;

    if (registeredAgent.agentPid > 0)
    {
        registeredAgent.isBusy = true;
        writeRegistration(registered.data(), registeredAgent);
    }
    else
    {
        unlink(registered.data());
    }

    registered[0] = 0;
}

void registerResponder(const std::string& store,
                       const std::string& name,
                       std::optional<int> agentPid)
{
    createStoreDirectory(store);
    auto registration = makeRegistration();
    registration.isResponder = true;
    registration.agentPid = agentPid.value_or(0);
    writeRegistration(makeRegistrationPath(store, name), registration);
}

void unregisterResponder(const std::string& store, const std::string& name)
{
    auto path = std::filesystem::path {store} / agentDirectory / name;

    if (readListenerProcessId(path) == static_cast<int>(getpid()))
        unlink(path.c_str());
}

bool isRunningResponder(const std::string& store, const std::string& name)
{
    auto path = std::filesystem::path {store} / agentDirectory / name;
    return readListenerProcessId(path) && readRegistration(path).isResponder;
}

bool isFinishingResponder(const std::string& store, const std::string& name)
{
    auto path = std::filesystem::path {store} / agentDirectory / name;
    return isFinishingResponder(readRegistration(path),
                                [](int pid) { return isAlive(pid); });
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
            activities.push_back(
                {path.fileName(), status.commentThread, status.doing});
    }

    return activities;
}
} // namespace diffview
