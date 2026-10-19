#include <Wake.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
ReviewFile makeRoundOwnedBy(const std::string& owner)
{
    auto comment = Annotation {};
    comment.id = 35;
    comment.path = "a.cpp";
    comment.fileLevel = true;
    comment.text = "still broken";
    comment.owner = owner;
    comment.draft = false;
    return {"2026-10-18-210000", "HEAD", "abc", {comment}};
}

std::map<std::string, KnownSession> makeSessions(bool isClaudeAlive)
{
    return {{"dev", {{"session-1", 4242, "/r/main"}, isClaudeAlive}}};
}
} // namespace

auto tWakeListening = test("Wake/a listening agent hears it: nobody is woken") = []
{
    auto decisions = decideWakes(
        makeRoundOwnedBy("dev"), {"ops"}, makeSessions(false), WakeMode::resume);

    check(decisions.size() == 1);
    check(decisions[0].kind == WakeKind::listening && decisions[0].agent == "ops");
    check(describeWakes(decisions) == "@ops listening");
};

auto tWakeResume =
    test("Wake/nobody listening resumes the owner's finished session") = []
{
    auto decisions = decideWakes(
        makeRoundOwnedBy("dev"), {}, makeSessions(false), WakeMode::resume);

    check(decisions.size() == 1);
    check(decisions[0].kind == WakeKind::resumed
          && decisions[0].sessionId == "session-1");
    check(describeWakes(decisions) == "woke @dev (resumed)");

    auto launch = makeWakeLaunch(decisions[0], {}, "prompt", "/r/main", "/r/log");
    check(launch.arguments.size() == 6);
    check(launch.arguments[2]
          == "claude -p --resume \"$2\" \"$1\" || claude -p \"$1\"");
    check(launch.arguments[4] == "prompt" && launch.arguments[5] == "session-1");
};

auto tWakeOpenSession =
    test("Wake/an open session isn't resumed: a fresh one is started") = []
{
    auto decisions = decideWakes(
        makeRoundOwnedBy("dev"), {}, makeSessions(true), WakeMode::resume);

    check(decisions[0].kind == WakeKind::freshBesideOpenSession);
    check(describeWakes(decisions) == "woke @dev (fresh: its session is open)");

    auto launch = makeWakeLaunch(decisions[0], {}, "prompt", "/r/main", "/r/log");
    check(launch.arguments.size() == 5 && launch.arguments[2] == "claude -p \"$1\"");
};

auto tWakeNoSession =
    test("Wake/an owner with no known session gets a fresh one") = []
{
    auto decisions = decideWakes(makeRoundOwnedBy("dev"), {}, {}, WakeMode::resume);

    check(decisions[0].kind == WakeKind::freshWithoutSession);
};

auto tWakeOff = test("Wake/wake = off wakes nobody and says so") = []
{
    auto decisions =
        decideWakes(makeRoundOwnedBy("dev"), {}, makeSessions(false), WakeMode::off);

    check(decisions[0].kind == WakeKind::off);
    check(describeWakes(decisions) == "nobody listening, wake off");
};

auto tWakeUnowned =
    test("Wake/an unowned thread with nobody listening wakes no one") = []
{
    check(decideWakes(makeRoundOwnedBy({}), {}, {}, WakeMode::resume).empty());
    check(describeWakes({}) == "nobody listening");
};

auto tWakeConfig = test("Wake/the repo's config overrides the user's") = []
{
    auto user =
        parseWakeConfig("wake = fresh\nwakeCommand = /bin/stub --flag\n", {});
    auto repo = parseWakeConfig("# comment\nwake=off\n", user);

    check(user.mode == WakeMode::fresh && user.command == "/bin/stub --flag");
    check(repo.mode == WakeMode::off && repo.command == "/bin/stub --flag");
    check(parseWakeConfig("wake = sideways\n", {}).mode == WakeMode::resume);
};

auto tAgentSession = test("Wake/an agent's session reads back") = []
{
    auto session =
        parseAgentSession(formatAgentSession({"abc-123", 77, "/r/main perf"}));

    check(session.sessionId == "abc-123" && session.claudePid == 77);
    check(session.worktree == "/r/main perf");
};
