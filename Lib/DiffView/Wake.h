#pragma once

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
    std::string command = "claude --dangerously-skip-permissions";
};

// "key = value" lines, as git's config reads; later files override earlier.
WakeConfig parseWakeConfig(const std::string& text, WakeConfig config);

// The Claude Code session an agent last monitored from, kept after it stops.
struct AgentSession
{
    std::string sessionId;
    int claudePid = 0;
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
};

struct KnownSession
{
    AgentSession session;
    bool isClaudeAlive = false;
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

// /bin/sh runs the configured command, so it may carry its own arguments; a
// failed resume falls back to a fresh session.
WakeLaunch makeWakeLaunch(const WakeDecision& decision,
                          const WakeConfig& config,
                          const std::string& prompt,
                          const std::string& worktree,
                          const std::string& logPath);
} // namespace diffview
