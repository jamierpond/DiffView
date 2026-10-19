#include <Activity.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

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
    check(isActivityFile("responder/dv.log"));
    check(isActivityFile("responder/dv.session"));
    check(isActivityFile("status/dev"));
    check(!isActivityFile("reviews/2026-10-18.md"));
    check(!isActivityFile("worktrees/feature/draft.md"));
    check(!isActivityFile("Source/responder/x"));
};
