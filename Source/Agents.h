#pragma once

#include <Activity.h>
#include <Wake.h>

#include <eacp/Core/Utils/FilePath.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
void registerAgent(const std::string& store,
                   const std::string& name,
                   const AgentSession& session);

// Still counted as listening for a while: it is acting on what woke it.
void markAgentStopped();
void unregisterAgent();

std::vector<std::string> findListeningAgents(const std::string& store);

std::map<std::string, KnownSession> readKnownSessions(const std::string& store);

void launchDetached(const WakeLaunch& launch);

// Follows this worktree's wake logs as they grow, and the `dv status` files
// listening agents leave in the store.
class ActivityWatcher
{
public:
    ActivityWatcher(eacp::FilePath wakeLogs,
                    eacp::FilePath statuses,
                    std::string worktree);

    std::vector<AgentActivity> findActivities(std::time_t now);

private:
    struct FollowedLog
    {
        WakeProgress progress;
        std::uint64_t offset = 0;
        bool hasBeenRead = false;
        bool isOver = false;
        std::time_t endedAt = 0;
    };

    void follow(const eacp::FilePath& path, FollowedLog& log, std::time_t now);
    void addWakeActivities(std::vector<AgentActivity>& activities, std::time_t now);
    void addStatusActivities(std::vector<AgentActivity>& activities,
                             std::time_t now);

    eacp::FilePath wakeLogs;
    eacp::FilePath statuses;
    std::string worktree;
    std::map<std::string, FollowedLog> logs;
};
} // namespace diffview
