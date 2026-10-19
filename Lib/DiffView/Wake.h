#pragma once

#include "AgentCli.h"
#include "Review.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
constexpr auto sessionDirectory = "sessions";
constexpr auto wakeDirectory = "wakes";
constexpr auto configFileName = "config";

enum class WakeMode
{
    resume,
    fresh,
    off
};

struct WakeConfig
{
    WakeMode mode = WakeMode::resume;
    // The CLI for agents whose `dv monitor` didn't say which they run in.
    std::string agentCli {defaultAgentCliName};
    // Replaces agentCli's unattended command; empty keeps it.
    std::string command;
};

// "key = value" lines, as git's config reads; later files override earlier.
WakeConfig parseWakeConfig(const std::string& text, WakeConfig config);

// The agent CLI session an agent last monitored from, kept after it stops.
struct AgentSession
{
    std::string agentCli;
    std::string sessionId;
    int agentPid = 0;
    std::string worktree;
};

std::string formatAgentSession(const AgentSession& session);
AgentSession parseAgentSession(const std::string& text);

enum class WakeKind
{
    listening,
    resumed,
    freshWithoutSession,
    freshBesideOpenSession,
    fresh,
    off
};

struct WakeDecision
{
    WakeKind kind = WakeKind::off;
    std::string agent;
    std::string sessionId;
    std::vector<int> threads;
    // Empty when the agent's CLI isn't known.
    std::string agentCli;
};

struct KnownSession
{
    AgentSession session;
    bool isAgentAlive = false;
};

// Nothing for an owner someone listening will hear; otherwise a wake per
// owner of a thread in the round. Unowned threads with nobody listening wake
// no one: there is no name to answer as.
std::vector<WakeDecision>
    decideWakes(const ReviewFile& round,
                const std::vector<std::string>& listening,
                const std::map<std::string, KnownSession>& sessions,
                WakeMode mode);

std::string describeWakes(const std::vector<WakeDecision>& decisions);

// The line `dv monitor --as <agent>` would have printed.
std::string makeWakeNotice(const ReviewFile& round,
                           const std::string& agent,
                           const std::string& path);

std::string makeWakePrompt(const std::string& notice, const std::string& agent);

struct WakeLaunch
{
    std::string worktree;
    std::string logPath;
    std::vector<std::string> arguments;
};

// The agent's own CLI if known, else the configured one; wakeCommand only
// replaces the configured one's command. /bin/sh runs it, so it may carry its
// own arguments; a failed resume falls back to a fresh session. The log opens
// with the shell's pid, the CLI and the threads, and ends with the exit status
// (see Activity.h).
WakeLaunch makeWakeLaunch(const WakeDecision& decision,
                          const WakeConfig& config,
                          const std::string& prompt,
                          const std::string& worktree,
                          const std::string& logPath);
} // namespace diffview
