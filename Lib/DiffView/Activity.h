#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
constexpr auto statusDirectory = "status";

// A woken agent's log opens with this line and ends with the exit one; the
// stream-json events come between.
constexpr auto wakeHeaderWord = std::string_view {"dv-wake"};
constexpr auto wakeExitWord = std::string_view {"dv-wake-exit"};

// What one stream-json event says the agent is doing; nothing for events that
// say nothing new (the final result, a system note).
std::optional<std::string> summariseStreamEvent(std::string_view line,
                                                const std::string& worktree);

struct WakeProgress
{
    int pid = 0;
    std::string agent;
    std::vector<int> threads;
    std::string doing = "starting";
    std::optional<int> exitCode;
    std::string partialLine;
};

// Folds more of a wake's log in; a last line without its newline waits for
// the rest.
void readWakeLog(WakeProgress& progress,
                 std::string_view text,
                 const std::string& worktree);

// What a listening agent said with `dv status`.
struct AgentStatus
{
    int thread = 0;
    std::time_t updated = 0;
    std::string doing;
};

constexpr auto statusExpirySeconds = std::time_t {300};

std::string formatAgentStatus(const AgentStatus& status);
AgentStatus parseAgentStatus(const std::string& text);
bool isAgentStatusCurrent(const AgentStatus& status, std::time_t now);

struct AgentActivity
{
    std::string agent;
    int thread = 0;
    std::string doing;
    bool hasFailed = false;
};

std::string describeActivity(const AgentActivity& activity);

// Logs and status files change all the time and never change the diff.
bool isActivityFile(std::string_view pathInStore);
} // namespace diffview
