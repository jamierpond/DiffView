#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace diffview
{
constexpr auto backgroundAgentHookName = std::string_view {"background-agent"};
constexpr auto registerInboxHookName = std::string_view {"register-inbox"};

// Claude Code's PreToolUse answer to an Agent call: allowed, with its input
// unchanged but for run_in_background, forced on. Nothing for any other tool,
// or a payload that isn't a hook's.
std::optional<std::string> makeBackgroundAgentHookAnswer(std::string_view payload);

// The responder's --settings JSON: other sessions' messages accepted, so dv's
// posts to its inbox aren't held; `<dvExecutable> hook background-agent`
// before every Agent call; `<dvExecutable> hook register-inbox --as <name>`
// at session start.
std::string makeResponderSettings(const std::string& dvExecutable,
                                  const std::string& responderName);
} // namespace diffview
