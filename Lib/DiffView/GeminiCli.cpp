#include "AgentCli.h"
#include "StreamEvents.h"

namespace diffview
{
namespace
{
using Miro::Json::Value;

std::string summariseToolUse(const Value& event, const std::string& worktree)
{
    static const auto tools = ToolSummaries {
        {"run_shell_command", {"running", "command"}},
        {"read_file", {"reading", "file_path"}},
        {"read_many_files", {"reading", "include"}},
        {"replace", {"editing", "file_path"}},
        {"write_file", {"writing", "file_path"}},
        {"list_directory", {"listing", "dir_path"}},
        {"grep_search", {"searching for", "pattern"}},
        {"glob", {"finding", "pattern"}},
        {"web_fetch", {"fetching", "prompt"}},
        {"google_web_search", {"searching the web for", "query"}},
    };

    return summariseToolCall(tools,
                             getString(event, "tool_name"),
                             getField(event, "parameters"),
                             worktree);
}

// --output-format stream-json: init, message, tool_use, tool_result, error
// and result events. The model's text comes in fragments, so it reads as
// thinking rather than being shown.
std::optional<std::string> summariseGeminiStreamLine(std::string_view line,
                                                     const std::string& worktree)
{
    auto event = Miro::Json::getParsedValue(line);
    auto type = getString(event, "type");

    if (type == "tool_use")
        return summariseToolUse(event, worktree);

    if (type == "tool_result")
        return "thinking";

    if (type == "message" && getString(event, "role") == "assistant")
        return "thinking";

    if (type == "init")
        return "starting";

    return std::nullopt;
}
} // namespace

// Gemini reads no messages from stdin once started and resumes only "latest"
// or an index, not an id: each message is a fresh run.
AgentCli makeGeminiCli()
{
    return {
        "gemini",
        "gemini",
        {"gemini", "--yolo"},
        {"--output-format", "stream-json"},
        {},
        "-p",
        "",
        summariseGeminiStreamLine,
        [](std::string_view) { return false; },
    };
}
} // namespace diffview
