#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace diffview
{
constexpr auto backgroundAgentHookName = std::string_view {"background-agent"};

// Claude Code's PreToolUse answer to an Agent call: allowed, with its input
// unchanged but for run_in_background, forced on. Nothing for any other tool,
// or a payload that isn't a hook's.
std::optional<std::string> makeBackgroundAgentHookAnswer(std::string_view payload);

// The --settings JSON that runs `<dvExecutable> hook background-agent` before
// every Agent call.
std::string makeBackgroundAgentHookSettings(const std::string& dvExecutable);
} // namespace diffview
