#include <Responder.h>
#include <Store.h>
#include <StreamEvents.h>

#include <NanoTest/NanoTest.h>

#include <eacp/Core/Utils/Files.h>

#include <filesystem>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto firstFakePid = 1000;
constexpr auto crashExitCode = 1;

const auto claudeInit =
    std::string {R"({"type":"system","subtype":"init","session_id":"session-1"})"}
    + "\n";
const auto claudeResult =
    std::string {R"({"type":"result","subtype":"success","session_id":"session-1"})"}
    + "\n";

Annotation makeThread(int id, const std::string& owner)
{
    auto comment = Annotation {};
    comment.id = id;
    comment.path = "a.cpp";
    comment.fileLevel = true;
    comment.text = "still broken";
    comment.owner = owner;
    comment.draft = false;
    return comment;
}

ReviewFile makeRound(std::vector<Annotation> comments)
{
    auto round = ReviewFile {};
    round.title = "2026-10-18-210000";
    round.baseLabel = "HEAD";
    round.baseSha = "abc";
    round.comments = std::move(comments);
    return round;
}

struct FakeProcessState
{
    int pid = 0;
    AgentLaunch launch;
    std::string written;
    std::string output;
    std::optional<int> exitCode;
    bool isStopped = false;
};

class FakeAgentProcess final : public AgentProcess
{
public:
    explicit FakeAgentProcess(std::shared_ptr<FakeProcessState> stateToUse)
        : state(std::move(stateToUse))
    {
    }

    int getProcessId() const override { return state->pid; }

    bool writeInput(std::string_view text) override
    {
        state->written += text;
        return true;
    }

    std::string readAvailableOutput() override
    {
        return std::exchange(state->output, {});
    }

    std::optional<int> findExitCode() override { return state->exitCode; }

    void stop() override { state->isStopped = true; }

private:
    std::shared_ptr<FakeProcessState> state;
};

struct FakeLauncher
{
    std::vector<std::shared_ptr<FakeProcessState>> started;

    StartAgentProcess makeStart()
    {
        return [this](const AgentLaunch& launch)
        {
            auto state = std::make_shared<FakeProcessState>();
            state->pid = firstFakePid + static_cast<int>(started.size());
            state->launch = launch;
            started.push_back(state);
            return std::make_unique<FakeAgentProcess>(state);
        };
    }
};

eacp::FilePath makeEmptyDirectory(const std::string& name)
{
    auto dir = std::filesystem::temp_directory_path() / "DiffViewTests" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return eacp::FilePath {dir.string()};
}

ResponderSettings makeSettings(const eacp::FilePath& dir,
                               std::string_view cli = defaultAgentCliName)
{
    return {"dv", getAgentCli(cli), "/r/main", dir / "dv.log", dir / "dv.session"};
}

std::string readMessageText(const std::string& line)
{
    auto event = Miro::Json::getParsedValue(line);
    const auto& content = getField(getField(event, "message"), "content");

    if (!content.isArray() || content.asArray().empty())
        return {};

    return getString(content.asArray().front(), "text");
}

bool contains(const std::vector<std::string>& texts, const std::string& text)
{
    return std::find(texts.begin(), texts.end(), text) != texts.end();
}
} // namespace

auto tRouteListeningOwner =
    test("Responder/a listening owner's thread is its own, not the responder's") = []
{
    auto route = routeSend(makeRound({makeThread(4, "ops")}), {"ops", "dv"}, "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderThreads.empty());
    check(describeSendRoute(route, "dv") == "@ops listening");
};

auto tRouteNobodyListening =
    test("Responder/with nobody listening the responder takes unowned and "
         "unheard threads") = []
{
    auto route =
        routeSend(makeRound({makeThread(4, {}), makeThread(5, "ops")}), {}, "dv");

    check(route.listeners.empty());
    check(route.responderThreads == std::vector<int> {4, 5});
    check(describeSendRoute(route, "dv") == "@dv");
};

auto tRouteExternalTakesUnowned =
    test("Responder/a listening agent takes unowned threads and absent owners'") = []
{
    auto route = routeSend(
        makeRound({makeThread(4, {}), makeThread(5, "gone")}), {"ops"}, "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderThreads.empty());
};

auto tRouteResponderOwns =
    test("Responder/the responder keeps its own threads, running or not") = []
{
    auto round = makeRound({makeThread(4, "dv"), makeThread(5, "ops")});
    auto route = routeSend(round, {"ops"}, "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderThreads == std::vector<int> {4});
    check(describeSendRoute(route, "dv") == "@ops listening, @dv");

    auto mentioned = makeThread(4, "dv");
    mentioned.replies.push_back({reviewerName, "now", "@ops take this", true});
    check(routeSend(makeRound({mentioned}), {"ops"}, "dv").responderThreads.empty());
};

auto tRouteResponderOff = test("Responder/with no responder nothing goes to it") = []
{
    auto route = routeSend(makeRound({makeThread(4, {})}), {}, {});

    check(route.listeners.empty() && route.responderThreads.empty());
    check(describeSendRoute(route, {}) == "nobody listening");
};

auto tFrameUserMessage =
    test("Responder/a message is one stream-json user line, text escaped") = []
{
    auto text = makeResponderMessage("dv: reply 2026 on #4 · \"quoted\"", "dv");
    auto line = frameUserMessage(text);

    check(text
          == "dv: reply 2026 on #4 · \"quoted\"\n"
             "Handle it with dv comments/show/reply/resolve as dv. dv sends you the "
             "next one itself: don't run dv monitor.");
    check(line.ends_with("\n") && line.find('\n') == line.size() - 1);

    auto event = Miro::Json::getParsedValue(line);
    check(getString(event, "type") == "user");
    check(getString(getField(event, "message"), "role") == "user");
    check(readMessageText(line) == text);
};

auto tResponderConfig = test("Responder/the repo's config overrides the user's") = []
{
    auto user = parseResponderConfig("responder = off\nagent = gemini\n", {});
    auto repo =
        parseResponderConfig("# comment\nresponder=on\nresponderName = bot\n", user);

    check(!user.isEnabled && user.agentCli == "gemini" && user.name == "dv");
    check(repo.isEnabled && repo.agentCli == "gemini" && repo.name == "bot");
    check(parseResponderConfig("responder = maybe\n", {}).isEnabled);
    check(parseResponderConfig("agent = cursor\n", {}).agentCli == "claude");
    check(parseResponderConfig("responderName = you\n", {}).name == "dv");
};

auto tResponderOneProcess =
    test("Responder/one process takes every message, queued while it works") = []
{
    auto dir = makeEmptyDirectory("ResponderOneProcess");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    check(!responder.isRunning());
    responder.send({"first", {4}});
    check(launcher.started.size() == 1);

    auto& process = *launcher.started.front();
    check(process.launch.workingDirectory == "/r/main");
    check(process.launch.arguments
          == makeAgentArguments(getAgentCli("claude"), {}, {}));
    check(readMessageText(process.written) == "first");

    responder.send({"second", {5}});
    check(launcher.started.size() == 1);
    check(process.written.find("second") == std::string::npos);

    process.output = claudeInit;
    responder.poll();
    check(responder.findActivities().size() == 1);
    check(responder.findActivities().front().thread == 4);
    check(responder.findActivities().front().doing == "starting");

    process.output = claudeResult;
    responder.poll();
    check(launcher.started.size() == 1);
    check(process.written.find("second") != std::string::npos);
    check(responder.findActivities().front().thread == 5);
    check(responder.getProcessId() == firstFakePid);

    responder.stop();
    check(process.isStopped && !responder.isRunning());
};

auto tResponderResumes =
    test("Responder/its session id is kept and the next one resumes it") = []
{
    auto dir = makeEmptyDirectory("ResponderResumes");
    auto launcher = FakeLauncher {};

    {
        auto responder = Responder {makeSettings(dir), launcher.makeStart()};
        responder.send({"first", {4}});
        launcher.started.front()->output = claudeInit + claudeResult;
        responder.poll();
    }

    check(launcher.started.front()->isStopped);
    check(eacp::Files::readFile(dir / "dv.session") == "session-1\n");

    auto responder = Responder {makeSettings(dir), launcher.makeStart()};
    responder.send({"again", {4}});
    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(getAgentCli("claude"), "session-1", {}));
};

auto tResponderFailedResume =
    test("Responder/a session that can't be resumed is forgotten, not a crash") = []
{
    auto dir = makeEmptyDirectory("ResponderFailedResume");
    writeText(dir / "dv.session", "gone\n");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}});
    launcher.started.front()->output =
        R"({"type":"result","subtype":"error_during_execution","session_id":"gone"})"
        "\n";
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(getAgentCli("claude"), {}, {}));
    check(readMessageText(launcher.started.back()->written) == "first");
    check(!eacp::Files::exists(dir / "dv.session"));
    check(responder.takeNotices().empty() && !responder.getFailure());
};

auto tResponderCrashes = test(
    "Responder/a crash restarts it once with the message, a second stops it") = []
{
    auto dir = makeEmptyDirectory("ResponderCrashes");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}});
    launcher.started.front()->output = claudeInit;
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2);
    check(readMessageText(launcher.started.back()->written) == "first");
    check(responder.takeNotices()
          == std::vector<std::string> {"@dv crashed (exit 1), restarted"});

    launcher.started.back()->output = claudeInit;
    launcher.started.back()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2 && !responder.isRunning());
    check(responder.getFailure().has_value());
    check(
        responder.getFailure()->starts_with("@dv crashed twice (exit 1), stopped"));
    check(contains(responder.takeNotices(), *responder.getFailure()));
    check(responder.findActivities().empty());

    responder.send({"ignored", {5}});
    check(launcher.started.size() == 2);
};

auto tResponderIdleCrash =
    test("Responder/a crash while idle restarts it at once, ready for the next") = []
{
    auto dir = makeEmptyDirectory("ResponderIdleCrash");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}});
    launcher.started.front()->output = claudeInit + claudeResult;
    responder.poll();
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2 && responder.isRunning());
    check(launcher.started.back()->written.empty());
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(getAgentCli("claude"), "session-1", {}));
};

auto tResponderOneShot =
    test("Responder/a CLI without a stdin session runs once per message") = []
{
    auto dir = makeEmptyDirectory("ResponderOneShot");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir, "gemini"), launcher.makeStart()};

    responder.send({"first", {4}});
    responder.send({"second", {5}});
    check(launcher.started.size() == 1);
    check(launcher.started.front()->launch.arguments.back() == "first");
    check(launcher.started.front()->written.empty());

    launcher.started.front()->exitCode = 0;
    responder.poll();
    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments.back() == "second");
    check(responder.takeNotices().empty());
};

auto tResponderLog = test("Responder/its stream and what dv did go to its log") = []
{
    auto dir = makeEmptyDirectory("ResponderLog");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}});
    launcher.started.front()->output = claudeInit;
    responder.poll();

    auto log = eacp::Files::readFile(dir / "dv.log");
    check(log.find("dv: sending #4: first") != std::string::npos);
    check(log.find("dv: started pid 1000: claude") != std::string::npos);
    check(log.find(claudeInit) != std::string::npos);
    check(launcher.started.front()->launch.logPath == (dir / "dv.log").str());
};
