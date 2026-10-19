#include "AgentCli.h"
#include "StreamEvents.h"

namespace diffview
{
namespace
{
using Miro::Json::Value;

std::string summariseToolUse(const Value& block, const std::string& worktree)
{
    return summariseToolCall(getClaudeCodeTools(),
                             getString(block, "name"),
                             getField(block, "input"),
                             worktree);
}

std::optional<std::string> summariseAssistant(const Value& event,
                                              const std::string& worktree)
{
    const auto& content = getField(getField(event, "message"), "content");

    if (!content.isArray() || content.asArray().empty())
        return std::nullopt;

    const auto& block = content.asArray().back();
    auto type = getString(block, "type");

    if (type == "tool_use")
        return summariseToolUse(block, worktree);

    if (type == "thinking" || type == "redacted_thinking")
        return "thinking";

    if (type == "text")
    {
        if (auto said = shortenSummary(getString(block, "text")); !said.empty())
            return said;
    }

    return std::nullopt;
}

// --output-format stream-json --verbose: whole messages, one per line.
std::optional<std::string> summariseClaudeStreamLine(std::string_view line,
                                                     const std::string& worktree)
{
    auto event = Miro::Json::getParsedValue(line);
    auto type = getString(event, "type");

    if (type == "assistant")
        return summariseAssistant(event, worktree);

    if (type == "user")
        return "thinking";

    if (type == "system" && getString(event, "subtype") == "init")
        return "starting";

    return std::nullopt;
}
} // namespace

const ToolSummaries& getClaudeCodeTools()
{
    static const auto tools = ToolSummaries {
        {"Bash", {"running", "command"}},
        {"Read", {"reading", "file_path"}},
        {"Edit", {"editing", "file_path"}},
        {"MultiEdit", {"editing", "file_path"}},
        {"Write", {"writing", "file_path"}},
        {"NotebookEdit", {"editing", "notebook_path"}},
        {"Grep", {"searching for", "pattern"}},
        {"Glob", {"finding", "pattern"}},
        {"Agent", {"delegating:", "description"}},
        {"Task", {"delegating:", "description"}},
        {"WebFetch", {"fetching", "url"}},
        {"WebSearch", {"searching the web for", "query"}},
    };

    return tools;
}

AgentCli makeClaudeCodeCli()
{
    return {
        std::string {defaultAgentCliName},
        "claude",
        {"claude", "--dangerously-skip-permissions"},
        {"-p",
         "--output-format",
         "stream-json",
         "--verbose",
         "--forward-subagent-text"},
        {"--input-format", "stream-json", "--replay-user-messages"},
        "",
        "--resume",
        summariseClaudeStreamLine,
        [](std::string_view line)
        { return getString(Miro::Json::getParsedValue(line), "type") == "result"; },
    };
}
} // namespace diffview
