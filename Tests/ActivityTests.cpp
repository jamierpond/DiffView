#include <Activity.h>

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
    return summariseStreamEvent(line, worktree);
}
} // namespace

auto tActivityToolUse =
    test("Activity/a tool call is its name's verb and its key argument") = []
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
    test("Activity/a long or multi-line argument is cut to one short line") = []
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
    test("Activity/thinking, a tool result and text say what's going on") = []
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

auto tActivityQuiet =
    test("Activity/results, empty text and lines that aren't JSON say nothing") = []
{
    check(!summarise(R"({"type":"result","subtype":"success"})"));
    check(!summarise(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"  "}]}})"));
    check(!summarise("Error: not logged in"));
    check(!summarise(""));
};

auto tActivityWakeLog =
    test("Activity/a wake log gives the pid, threads, latest action and exit") = []
{
    auto progress = WakeProgress {};
    readWakeLog(progress, "dv-wake pid 4242 agent dev threads 46 47\n", worktree);

    check(progress.pid == 4242 && progress.agent == "dev");
    check(progress.threads == std::vector<int> {46, 47});
    check(progress.doing == "starting" && !progress.exitCode);

    auto event = makeToolUse("Bash", R"({"command":"dv show 46"})") + "\n";
    readWakeLog(progress, event.substr(0, 20), worktree);
    check(progress.doing == "starting");

    readWakeLog(progress, event.substr(20), worktree);
    check(progress.doing == "running dv show 46");

    readWakeLog(progress, "dv-wake-exit 1\n", worktree);
    check(progress.exitCode == 1);
};

auto tActivityStatusExpiry =
    test("Activity/a status is current until a few minutes pass unchanged") = []
{
    auto written = AgentStatus {46, 1000, "running the tests\nand more"};
    auto status = parseAgentStatus(formatAgentStatus(written));

    check(status.thread == 46 && status.updated == 1000);
    check(status.doing == "running the tests");
    check(isAgentStatusCurrent(status, 1000));
    check(isAgentStatusCurrent(status, 1000 + statusExpirySeconds - 1));
    check(!isAgentStatusCurrent(status, 1000 + statusExpirySeconds));
    check(!isAgentStatusCurrent(parseAgentStatus(""), 1000));
};

auto tActivityFiles =
    test("Activity/logs and status files are told from the review store") = []
{
    check(isActivityFile("wakes/2026-10-18-dev.log"));
    check(isActivityFile("worktrees/feature/wakes/2026-10-18-dev.log"));
    check(isActivityFile("status/dev"));
    check(!isActivityFile("reviews/2026-10-18.md"));
    check(!isActivityFile("worktrees/feature/draft.md"));
    check(!isActivityFile("Source/wakes/x"));
};
