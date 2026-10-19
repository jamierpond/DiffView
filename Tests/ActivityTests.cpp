#include <Activity.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto worktree = "/r/main";

std::string makeBashToolUse(const std::string& command)
{
    return R"({"type":"assistant","message":{"content":[{"type":"tool_use","name":"Bash","input":{"command":")"
           + command + R"("}}]}})";
}
} // namespace

auto tActivityWakeLog =
    test("Activity/a wake log gives the pid, threads, latest action and exit") = []
{
    auto progress = WakeProgress {};
    readWakeLog(
        progress, "dv-wake pid 4242 agent dev cli claude threads 46 47\n", worktree);

    check(progress.pid == 4242 && progress.agent == "dev"
          && progress.cli == "claude");
    check(progress.threads == std::vector<int> {46, 47});
    check(progress.doing == "starting" && !progress.exitCode);

    auto event = makeBashToolUse("dv show 46") + "\n";
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

auto tActivityWakeLogCli =
    test("Activity/a wake log is read with the CLI its header names") = []
{
    auto progress = WakeProgress {};
    readWakeLog(
        progress,
        "dv-wake pid 7 agent dev cli gemini threads 46\n"
        R"({"type":"tool_use","tool_name":"replace","parameters":{"file_path":"/r/main/a.cpp"}})"
        "\n",
        worktree);

    check(progress.cli == "gemini" && progress.doing == "editing a.cpp");
};
