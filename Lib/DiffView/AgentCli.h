#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
constexpr auto defaultAgentCliName = std::string_view {"claude"};

// What dv needs from an agent CLI to wake it and show what it is doing.
// Adding one: a file defining its make function, and a line in the registry
// in AgentCli.cpp.
struct AgentCli
{
    std::string name;
    // Every permission granted, so a wake never stops to ask; wakeCommand
    // replaces it.
    std::string unattendedCommand;
    // Headless, printing one JSON event per line.
    std::string headlessArguments;
    // Goes right before the prompt: empty when the prompt is positional.
    std::string promptFlag;
    // Followed by the session id; empty when the CLI can't resume one.
    std::string resumeFlag;
    // Set in the shells the CLI runs, so `dv monitor` knows what it runs in.
    std::string detectionVariable;
    std::string sessionIdVariable;
    std::string processIdVariable;
    // What one line of its event stream says it is doing; nothing for lines
    // that say nothing new.
    std::function<std::optional<std::string>(std::string_view line,
                                             const std::string& worktree)>
        summariseStreamLine;
};

const std::vector<AgentCli>& getAgentClis();
bool isKnownAgentCli(std::string_view name);

// The default CLI for an unknown name.
const AgentCli& getAgentCli(std::string_view name);

// The first CLI whose detection variable `readVariable` finds set.
std::optional<AgentCli> detectAgentCli(
    const std::function<std::string(const std::string&)>& readVariable);

bool canResume(const AgentCli& cli);

// Shell text running `command` headless on the prompt in `promptArgument`
// ("$1"), resuming the session in `sessionArgument` when there is one.
std::string makeAgentCommand(const AgentCli& cli,
                             const std::string& command,
                             const std::string& promptArgument,
                             const std::string& sessionArgument);
} // namespace diffview
