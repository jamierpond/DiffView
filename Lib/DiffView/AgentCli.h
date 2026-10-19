#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
constexpr auto defaultAgentCliName = std::string_view {"claude"};

// What dv needs from an agent CLI to run its responder and show what it is
// doing. Adding one: a file defining its make function, and a line in the
// registry in AgentCli.cpp.
struct AgentCli
{
    std::string name;
    // Its process's executable, or the script an interpreter runs.
    std::string processName;
    // Every permission granted: nobody is there to answer a prompt.
    std::vector<std::string> unattendedCommand;
    // Headless, printing one JSON event per line.
    std::vector<std::string> headlessArguments;
    // Keeps reading user messages, one JSON line each, from stdin; empty when
    // the CLI can't, and each message is its own run with the prompt after
    // promptFlag (or positional when that is empty).
    std::vector<std::string> stdinSessionArguments;
    std::string promptFlag;
    // Followed by the session id; empty when the CLI can't resume one by id.
    std::string resumeFlag;
    // What one line of its event stream says it is doing; nothing for lines
    // that say nothing new.
    std::function<std::optional<std::string>(std::string_view line,
                                             const std::string& worktree)>
        summariseStreamLine;
    // In a stdin session: the line that ends its answer to one message.
    std::function<bool(std::string_view line)> isEndOfTurn;
};

const std::vector<AgentCli>& getAgentClis();
bool isKnownAgentCli(std::string_view name);

// The default CLI for an unknown name.
const AgentCli& getAgentCli(std::string_view name);

struct ProcessInfo
{
    int parentPid = 0;
    std::vector<std::string> arguments;
};

using ReadProcessInfo = std::function<std::optional<ProcessInfo>(int pid)>;

// The nearest process at or above `pid` that is an agent CLI.
std::optional<int> findAgentCliAncestor(int pid, const ReadProcessInfo& readProcess);

bool canResume(const AgentCli& cli);
bool hasStdinSession(const AgentCli& cli);

// The command line starting the CLI headless: a stdin session, or one run on
// `prompt` for a CLI without one; resuming `sessionId` when there is one.
std::vector<std::string> makeAgentArguments(const AgentCli& cli,
                                            const std::string& sessionId,
                                            const std::string& prompt);
} // namespace diffview
