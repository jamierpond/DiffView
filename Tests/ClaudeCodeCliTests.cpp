#include <AgentCli.h>
#include <Hooks.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto worktree = "/r/main";
constexpr auto dvExecutable = "/bin/dv";

std::string makeToolUse(const std::string& name, const std::string& input)
{
    return R"({"type":"assistant","message":{"content":[{"type":"tool_use","name":")"
           + name + R"(","input":)" + input + "}]}}";
}

std::optional<std::string> summarise(const std::string& line)
{
    return getAgentCli("claude").summariseStreamLine(line, worktree);
}
} // namespace

auto tClaudeCodeCommand =
    test("ClaudeCodeCli/reads messages from stdin and resumes by id") = []
{
    const auto& cli = getAgentCli("claude");
    using Arguments = std::vector<std::string>;
    auto session = Arguments {"claude",
                              "--dangerously-skip-permissions",
                              "-p",
                              "--output-format",
                              "stream-json",
                              "--verbose",
                              "--forward-subagent-text",
                              "--input-format",
                              "stream-json",
                              "--replay-user-messages"};
    auto responderArguments =
        Arguments {"--settings",
                   makeBackgroundAgentHookSettings(dvExecutable),
                   "--append-system-prompt",
                   std::string {responderStandingOrders}};
    auto fresh = session;
    fresh.insert(fresh.end(), responderArguments.begin(), responderArguments.end());

    check(hasStdinSession(cli) && canResume(cli));
    check(makeAgentArguments(cli, dvExecutable, {}, "ignored") == fresh);

    session.insert(session.end(), {"--resume", "session-1"});
    session.insert(
        session.end(), responderArguments.begin(), responderArguments.end());
    check(makeAgentArguments(cli, dvExecutable, "session-1", "ignored") == session);
};

auto tClaudeCodeStandingOrders = test(
    "ClaudeCodeCli/the responder is told to keep subagents in the background") = []
{
    check(responderStandingOrders
          == "You are the interface: subagents do the work in the background, you "
             "end your turn and answer new rounds, replies and messages as they "
             "arrive; act on subagent results when notified. Steer a running "
             "subagent with SendMessage.");
};

auto tClaudeCodeEndOfTurn = test("ClaudeCodeCli/a result line ends a turn") = []
{
    const auto& cli = getAgentCli("claude");

    check(cli.isEndOfTurn(R"({"type":"result","subtype":"success"})"));
    check(!cli.isEndOfTurn(R"({"type":"assistant","message":{}})"));
    check(!cli.isEndOfTurn("not json"));
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
