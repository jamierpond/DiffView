#pragma once

#include <ctime>
#include <string>
#include <string_view>

namespace diffview
{
constexpr auto statusDirectory = "status";

// What a listening agent said with `dv status`.
struct AgentStatus
{
    int commentThread = 0;
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
    int commentThread = 0;
    std::string doing;
};

std::string describeActivity(const AgentActivity& activity);

// Logs and status files change all the time and never change the diff.
bool isActivityFile(std::string_view pathInStore);
} // namespace diffview
