#include <AgentCli.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto worktree = "/r/main";

std::string makeToolUse(const std::string& name, const std::string& input)
{
    return R"({"type":"assistant","message":{"content":[{"type":"tool_use","name":")"
           + name + R"(","input":)" + input + "}]}}";
}

std::optional<std::string> summarise(const std::string& line)
{
    return getAgentCli("claude").summariseStreamLine(line, worktree);
}

std::string readVariableInClaudeCode(const std::string& name)
{
    if (name == "CLAUDECODE")
        return "1";

    return {};
}
} // namespace

auto tClaudeCodeCommand =
    test("ClaudeCodeCli/runs headless on a positional prompt and resumes by id") = []
{
    const auto& cli = getAgentCli("claude");

    check(makeAgentCommand(cli, cli.unattendedCommand, "\"$1\"", {})
          == "claude --dangerously-skip-permissions -p --output-format stream-json "
             "--verbose \"$1\"");
    check(makeAgentCommand(cli, "/bin/stub", "\"$1\"", "\"$4\"")
          == "/bin/stub -p --output-format stream-json --verbose --resume \"$4\" "
             "\"$1\"");
};

auto tClaudeCodeDetected = test("ClaudeCodeCli/is detected by CLAUDECODE") = []
{
    auto cli = detectAgentCli(readVariableInClaudeCode);

    check(cli && cli->name == "claude");
    check(cli->sessionIdVariable == "CLAUDE_CODE_SESSION_ID");
    check(!detectAgentCli([](const std::string&) { return std::string {}; }));
};

auto tActivityToolUse =
    test("ClaudeCodeCli/a tool call is its name's verb and its key argument") = []
{
    check(summarise(makeToolUse("Bash", R"({"command":"dv show 46"})"))
          == "running dv show 46");
    check(summarise(makeToolUse("Edit", R"({"file_path":"/r/main/Source/Foo.cpp"})"))
          == "editing Source/Foo.cpp");
    check(summarise(makeToolUse("Read", R"({"file_path":"/elsewhere/a.h"})"))
          == "reading /elsewhere/a.h");
    check(summarise(makeToolUse("Grep", R"({"pattern":"wakeDirectory"})"))
          == "searching for wakeDirectory");
    check(summarise(makeToolUse("mcp__x__y", "{}")) == "using mcp__x__y");
};

auto tActivityLongCommand =
    test("ClaudeCodeCli/a long or multi-line argument is cut to one short line") = []
{
    constexpr auto paddingLength = 80;
    auto command = std::string {"for f in a b c; do echo $f; done && "}
                   + std::string(paddingLength, 'x');
    auto summary =
        summarise(makeToolUse("Bash", R"({"command":")" + command + R"("})"));

    check(summary && summary->ends_with("…") && summary->size() < command.size());
    check(summarise(makeToolUse("Bash", R"({"command":"make\nmake test"})"))
          == "running make…");
};

auto tActivityThinkingAndText =
    test("ClaudeCodeCli/thinking, a tool result and text say what's going on") = []
{
    check(
        summarise(
            R"({"type":"assistant","message":{"content":[{"type":"thinking","thinking":"hmm"}]}})")
        == "thinking");
    check(summarise(
              R"({"type":"user","message":{"content":[{"type":"tool_result"}]}})")
          == "thinking");
    check(
        summarise(
            R"({"type":"assistant","message":{"content":[{"type":"text","text":"Fixed it."}]}})")
        == "Fixed it.");
    check(summarise(R"({"type":"system","subtype":"init"})") == "starting");
};

auto tActivityQuiet = test(
    "ClaudeCodeCli/results, empty text and lines that aren't JSON say nothing") = []
{
    check(!summarise(R"({"type":"result","subtype":"success"})"));
    check(!summarise(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"  "}]}})"));
    check(!summarise("Error: not logged in"));
    check(!summarise(""));
};
