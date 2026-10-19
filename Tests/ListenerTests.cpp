#include <AgentCli.h>
#include <Listener.h>
#include <Responder.h>

#include <NanoTest/NanoTest.h>

#include <map>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto monitorPid = 300;
constexpr auto shellPid = 200;
constexpr auto agentPid = 100;
constexpr auto launchdPid = 1;
constexpr auto commentThreadId = 4;

ListenerRegistration makeBusyAgent()
{
    auto registration = ListenerRegistration {};
    registration.monitorPid = monitorPid;
    registration.agentPid = agentPid;
    registration.started = "2026-10-18-233000";
    registration.isBusy = true;
    return registration;
}

std::function<bool(int)> makeLiveSet(std::vector<int> live)
{
    return [live](int pid)
    { return std::find(live.begin(), live.end(), pid) != live.end(); };
}

ReadProcessInfo makeProcessTable(std::map<int, ProcessInfo> table)
{
    return [table](int pid) -> std::optional<ProcessInfo>
    {
        if (auto found = table.find(pid); found != table.end())
            return found->second;

        return std::nullopt;
    };
}
} // namespace

auto tRegistrationReadsBack = test("Listener/a registration reads back") = []
{
    auto parsed =
        parseListenerRegistration(formatListenerRegistration(makeBusyAgent()));

    check(parsed.monitorPid == monitorPid && parsed.agentPid == agentPid);
    check(parsed.started == "2026-10-18-233000");
    check(parsed.isBusy && !parsed.isResponder);
    check(parseListenerRegistration("pid 7\nstarted x\nresponder\n").isResponder);
};

auto tRegistrationInboxReadsBack =
    test("Listener/a registration's inbox socket and token read back") = []
{
    auto registration = makeBusyAgent();
    registration.inbox = {"/tmp/run dir/cc-socks/42.sock", "08897a020b8f"};
    auto text = formatListenerRegistration(registration);
    auto parsed = parseListenerRegistration(text);

    check(text.find("inbox /tmp/run dir/cc-socks/42.sock\n") != std::string::npos);
    check(text.find("inbox-token 08897a020b8f\n") != std::string::npos);
    check(parsed.inbox.socketPath == "/tmp/run dir/cc-socks/42.sock");
    check(parsed.inbox.token == "08897a020b8f");
    check(parsed.agentPid == agentPid && parsed.isBusy);
};

auto tRegistrationWithoutInbox =
    test("Listener/a registration without an inbox writes no inbox lines") = []
{
    auto text = formatListenerRegistration(makeBusyAgent());

    check(text.find("inbox") == std::string::npos);
    check(!hasInboxSocket(parseListenerRegistration(text).inbox));
};

auto tMonitorListens = test("Listener/a running monitor listens") = []
{
    auto registration = makeBusyAgent();
    registration.isBusy = false;

    check(findLiveListenerPid(registration, makeLiveSet({monitorPid}))
          == monitorPid);
    check(!findLiveListenerPid(registration, makeLiveSet({agentPid})));
};

auto tBusyAgentListens = test(
    "Listener/after --once the agent listens until its CLI process is gone") = []
{
    auto registration = makeBusyAgent();

    check(findLiveListenerPid(registration, makeLiveSet({agentPid})) == agentPid);
    check(!findLiveListenerPid(registration, makeLiveSet({})));
    check(!findLiveListenerPid(registration, makeLiveSet({monitorPid})));

    registration.agentPid = 0;
    check(!findLiveListenerPid(registration, makeLiveSet({agentPid})));
};

auto tBusyAgentKeepsItsThreads =
    test("Listener/a busy agent's threads wait for it, not the responder") = []
{
    auto isListening = findLiveListenerPid(makeBusyAgent(), makeLiveSet({agentPid}));
    auto listening = std::vector<std::string> {};

    if (isListening)
        listening.push_back("dev");

    auto comment = Annotation {};
    comment.id = commentThreadId;
    comment.path = "a.cpp";
    comment.fileLevel = true;
    comment.owner = "dev";
    comment.draft = false;
    comment.replies.push_back({reviewerName, "now", "and this?", true});
    auto round = ReviewFile {};
    round.title = "2026-10-18-233100";
    round.comments = {comment};
    round.reply = true;

    auto route = routeSend(round, listening, "dv");
    check(route.listeners == std::vector<std::string> {"dev"});
    check(route.responderCommentThreads.empty());

    auto gone = routeSend(round, {}, "dv");
    check(gone.responderCommentThreads == std::vector<int> {commentThreadId});
};

auto tAgentCliAncestor =
    test("Listener/the agent is the nearest claude or gemini above dv") = []
{
    auto claudeTree = makeProcessTable({
        {monitorPid, {shellPid, {"dv", "monitor", "--once"}}},
        {shellPid, {agentPid, {"/bin/zsh", "-c", "dv monitor"}}},
        {agentPid, {launchdPid, {"claude", "--dangerously-skip-permissions"}}},
    });
    auto geminiTree = makeProcessTable({
        {shellPid, {agentPid, {"bash", "-c", "dv monitor"}}},
        {agentPid,
         {launchdPid, {"/opt/homebrew/bin/node", "/opt/homebrew/bin/gemini"}}},
    });
    auto plainShell = makeProcessTable({
        {shellPid, {launchdPid, {"-zsh"}}},
    });

    check(findAgentCliAncestor(shellPid, claudeTree) == agentPid);
    check(findAgentCliAncestor(shellPid, geminiTree) == agentPid);
    check(!findAgentCliAncestor(shellPid, plainShell));
    check(!findAgentCliAncestor(shellPid, makeProcessTable({})));
};

auto tFinishingResponderHolds = test(
    "Listener/after its dv quits the responder's CLI holds it until it ends") = []
{
    auto registration = ListenerRegistration {};
    registration.monitorPid = monitorPid;
    registration.agentPid = agentPid;
    registration.isResponder = true;

    check(findLiveListenerPid(registration, makeLiveSet({monitorPid, agentPid}))
          == monitorPid);
    check(!isFinishingResponder(registration, makeLiveSet({monitorPid, agentPid})));

    check(findLiveListenerPid(registration, makeLiveSet({agentPid})) == agentPid);
    check(isFinishingResponder(registration, makeLiveSet({agentPid})));

    check(!findLiveListenerPid(registration, makeLiveSet({})));
    check(!isFinishingResponder(registration, makeLiveSet({})));

    registration.isResponder = false;
    check(!findLiveListenerPid(registration, makeLiveSet({agentPid})));
};
