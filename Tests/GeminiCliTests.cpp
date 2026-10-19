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
} // namespace

auto tGeminiCommand =
    test("GeminiCli/has no stdin session: each message is a fresh run after -p") = []
{
    const auto& cli = getAgentCli("gemini");
    using Arguments = std::vector<std::string>;

    check(!hasStdinSession(cli) && !canResume(cli));
    check(makeAgentArguments(cli, "session-1", "the prompt")
          == Arguments {"gemini",
                        "--yolo",
                        "--output-format",
                        "stream-json",
                        "-p",
                        "the prompt"});
    check(!cli.isEndOfTurn(R"({"type":"result","status":"success"})"));
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
