#include <AgentCli.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto worktree = "/r/main";

// Event shapes from gemini-cli 0.46's stream-json formatter.
std::string makeToolUse(const std::string& name, const std::string& parameters)
{
    return R"({"type":"tool_use","timestamp":"2026-10-18T23:00:00.000Z","tool_name":")"
           + name + R"(","tool_id":"t1","parameters":)" + parameters + "}";
}

std::optional<std::string> summarise(const std::string& line)
{
    return getAgentCli("gemini").summariseStreamLine(line, worktree);
}

std::string readVariableInGemini(const std::string& name)
{
    if (name == "GEMINI_CLI")
        return "1";

    return {};
}
} // namespace

auto tGeminiCommand =
    test("GeminiCli/runs headless with the prompt after -p, yolo by default") = []
{
    const auto& cli = getAgentCli("gemini");

    check(makeAgentCommand(cli, cli.unattendedCommand, "\"$1\"", {})
          == "gemini --yolo --output-format stream-json -p \"$1\"");
    check(makeAgentCommand(cli, cli.unattendedCommand, "\"$1\"", "\"$4\"")
          == "gemini --yolo --output-format stream-json --resume \"$4\" -p \"$1\"");
};

auto tGeminiDetected =
    test("GeminiCli/is detected by GEMINI_CLI, with no session id to record") = []
{
    auto cli = detectAgentCli(readVariableInGemini);

    check(cli && cli->name == "gemini");
    check(cli->sessionIdVariable.empty() && cli->processIdVariable.empty());
};

auto tGeminiToolUse =
    test("GeminiCli/a tool call is its name's verb and its key argument") = []
{
    check(summarise(makeToolUse("run_shell_command", R"({"command":"dv show 46"})"))
          == "running dv show 46");
    check(summarise(
              makeToolUse("replace", R"({"file_path":"/r/main/Source/Foo.cpp"})"))
          == "editing Source/Foo.cpp");
    check(summarise(makeToolUse("read_file", R"({"file_path":"/elsewhere/a.h"})"))
          == "reading /elsewhere/a.h");
    check(summarise(makeToolUse("grep_search", R"({"pattern":"wakeDirectory"})"))
          == "searching for wakeDirectory");
    check(summarise(makeToolUse("save_memory", "{}")) == "using save_memory");
};

auto tGeminiProgress =
    test("GeminiCli/init starts, the model's fragments and tool results think") = []
{
    check(
        summarise(
            R"({"type":"init","timestamp":"t","session_id":"s","model":"gemini-2.5-pro"})")
        == "starting");
    check(
        summarise(
            R"({"type":"message","timestamp":"t","role":"assistant","content":"Fix","delta":true})")
        == "thinking");
    check(
        summarise(
            R"({"type":"tool_result","timestamp":"t","tool_id":"t1","status":"success","output":""})")
        == "thinking");
};

auto tGeminiQuiet =
    test("GeminiCli/the prompt, results and lines that aren't JSON say nothing") = []
{
    check(!summarise(
        R"({"type":"message","timestamp":"t","role":"user","content":"handle it"})"));
    check(!summarise(R"({"type":"result","timestamp":"t","status":"success"})"));
    check(!summarise("YOLO mode is enabled."));
    check(!summarise(""));
};
