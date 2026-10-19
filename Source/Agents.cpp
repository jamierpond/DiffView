#include "Agents.h"

#include <Review.h>
#include <Store.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <eacp/Core/Utils/Files.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace diffview
{
namespace
{
auto registered = std::array<char, 4096> {};

// An agent between `dv monitor --once` runs is still working its threads.
constexpr auto listeningGraceSeconds = std::time_t {600};

constexpr auto failureShownSeconds = std::time_t {10};

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

std::string describeExit(std::optional<int> exitCode)
{
    if (!exitCode)
        return "stopped";

    return "exit " + std::to_string(*exitCode);
}
} // namespace

void registerAgent(const std::string& store,
                   const std::string& name,
                   const AgentSession& session)
{
    createStoreDirectory(store);

    if (!session.agentCli.empty())
        writeText(eacp::FilePath {store} / sessionDirectory / name,
                  formatAgentSession(session));

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

void unregisterAgent()
{
    if (registered[0] != 0)
        unlink(registered.data());

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

std::map<std::string, KnownSession> readKnownSessions(const std::string& store)
{
    auto sessions = std::map<std::string, KnownSession> {};

    for (const auto& path:
         eacp::Files::listFiles(eacp::FilePath {store} / sessionDirectory))
    {
        auto session = parseAgentSession(eacp::Files::readFile(path));
        sessions[path.fileName()] = {session, isAlive(session.agentPid)};
    }

    return sessions;
}

// Double-forked into its own session, so it outlives the UI and leaves no
// zombie; only async-signal-safe calls between fork and exec.
void launchDetached(const WakeLaunch& launch)
{
    eacp::Files::createDirectories(
        eacp::FilePath {launch.logPath}.parentDirectory());

    auto arguments = std::vector<char*> {};

    for (const auto& argument: launch.arguments)
        arguments.push_back(const_cast<char*>(argument.c_str()));

    arguments.push_back(nullptr);

    auto child = fork();

    if (child < 0)
        return;

    if (child > 0)
    {
        waitpid(child, nullptr, 0);
        return;
    }

    setsid();

    if (fork() != 0)
        _exit(0);

    auto input = open("/dev/null", O_RDONLY);
    auto log = open(launch.logPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    dup2(input, STDIN_FILENO);
    dup2(log, STDOUT_FILENO);
    dup2(log, STDERR_FILENO);

    if (chdir(launch.worktree.c_str()) != 0)
        _exit(1);

    execv(arguments[0], arguments.data());
    _exit(127);
}

ActivityWatcher::ActivityWatcher(eacp::FilePath wakeLogsToUse,
                                 eacp::FilePath statusesToUse,
                                 std::string worktreeToUse)
    : wakeLogs(std::move(wakeLogsToUse))
    , statuses(std::move(statusesToUse))
    , worktree(std::move(worktreeToUse))
{
}

std::vector<AgentActivity> ActivityWatcher::findActivities(std::time_t now)
{
    auto activities = std::vector<AgentActivity> {};
    addWakeActivities(activities, now);
    addStatusActivities(activities, now);
    return activities;
}

// A log already over when first seen ended before this UI started: it is
// never shown, and never read again.
void ActivityWatcher::follow(const eacp::FilePath& path,
                             FollowedLog& log,
                             std::time_t now)
{
    auto isFirstRead = !log.hasBeenRead;
    log.hasBeenRead = true;
    auto added = readFileFromByteOffset(path, log.offset);
    log.offset += added.size();
    readWakeLog(log.progress, added, worktree);

    auto hasHeader = log.progress.pid > 0;
    auto hasEnded = log.progress.exitCode.has_value()
                    || (hasHeader && !isAlive(log.progress.pid));
    auto predatesHeaders = isFirstRead && !added.empty() && !hasHeader;

    if (!hasEnded && !predatesHeaders)
        return;

    log.isOver = true;

    if (!isFirstRead)
        log.endedAt = now;
}

void ActivityWatcher::addWakeActivities(std::vector<AgentActivity>& activities,
                                        std::time_t now)
{
    auto options = eacp::Files::DirectoryOptions {};
    options.pattern = "*.log";

    for (const auto& path: eacp::Files::listFiles(wakeLogs, options))
    {
        auto& log = logs[path.str()];

        if (!log.isOver)
            follow(path, log, now);

        const auto& progress = log.progress;
        auto hasFailed = !progress.exitCode || *progress.exitCode != 0;
        auto isShowingFailure = log.isOver && hasFailed && log.endedAt > 0
                                && now - log.endedAt < failureShownSeconds;

        if (log.isOver && !isShowingFailure)
            continue;

        auto doing =
            isShowingFailure ? describeExit(progress.exitCode) : progress.doing;

        for (auto thread: progress.threads)
            activities.push_back({progress.agent, thread, doing, isShowingFailure});
    }
}

void ActivityWatcher::addStatusActivities(std::vector<AgentActivity>& activities,
                                          std::time_t now)
{
    for (const auto& path: eacp::Files::listFiles(statuses))
    {
        auto status = parseAgentStatus(eacp::Files::readFile(path));
        auto agent = path.fileName();
        auto isWoken = std::any_of(
            activities.begin(),
            activities.end(),
            [&](const AgentActivity& activity)
            { return activity.agent == agent && activity.thread == status.thread; });

        if (isAgentStatusCurrent(status, now) && !isWoken)
            activities.push_back({agent, status.thread, status.doing, false});
    }
}
} // namespace diffview
