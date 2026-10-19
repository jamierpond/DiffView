#include <AgentProcess.h>
#include <Responder.h>
#include <Store.h>
#include <StreamEvents.h>

#include <NanoTest/NanoTest.h>

#include <eacp/Core/Utils/Base64.h>
#include <eacp/Core/Utils/Files.h>

#include <chrono>
#include <filesystem>
#include <thread>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto firstFakePid = 1000;
constexpr auto crashExitCode = 1;
constexpr auto dvExecutable = "/bin/dv";

const auto claudeInit =
    std::string {R"({"type":"system","subtype":"init","session_id":"session-1"})"}
    + "\n";
const auto claudeResult =
    std::string {R"({"type":"result","subtype":"success","session_id":"session-1"})"}
    + "\n";

// What Claude Code writes back on taking a message, with --replay-user-messages.
std::string makeEcho(const std::string& text)
{
    return frameUserMessage(text, {});
}

Annotation makeCommentThread(int id, const std::string& owner)
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
    std::optional<int> exitCode;
    bool isInputClosed = false;
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

    std::optional<int> findExitCode() override { return state->exitCode; }

    void closeInput() override { state->isInputClosed = true; }

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
    return {"dv",
            getAgentCli(cli),
            "/r/main",
            dir / "dv.log",
            dir / "dv.jsonl",
            dir / "dv.session",
            dvExecutable};
}

void writeOutput(const eacp::FilePath& dir, const std::string& text)
{
    eacp::Files::appendFile(dir / "dv.jsonl", text);
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
    auto route =
        routeSend(makeRound({makeCommentThread(4, "ops")}), {"ops", "dv"}, "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderCommentThreads.empty());
    check(describeSendRoute(route, "dv") == "@ops listening");
};

auto tRouteNobodyListening =
    test("Responder/with nobody listening the responder takes unowned and "
         "unheard threads") = []
{
    auto route =
        routeSend(makeRound({makeCommentThread(4, {}), makeCommentThread(5, "ops")}),
                  {},
                  "dv");

    check(route.listeners.empty());
    check(route.responderCommentThreads == std::vector<int> {4, 5});
    check(describeSendRoute(route, "dv") == "@dv");
};

auto tRouteExternalTakesUnowned =
    test("Responder/a listening agent takes unowned threads and absent owners'") = []
{
    auto route = routeSend(
        makeRound({makeCommentThread(4, {}), makeCommentThread(5, "gone")}),
        {"ops"},
        "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderCommentThreads.empty());
};

auto tRouteResponderOwns =
    test("Responder/the responder keeps its own threads, running or not") = []
{
    auto round =
        makeRound({makeCommentThread(4, "dv"), makeCommentThread(5, "ops")});
    auto route = routeSend(round, {"ops"}, "dv");

    check(route.listeners == std::vector<std::string> {"ops"});
    check(route.responderCommentThreads == std::vector<int> {4});
    check(describeSendRoute(route, "dv") == "@ops listening, @dv");

    auto mentioned = makeCommentThread(4, "dv");
    mentioned.replies.push_back({reviewerName, "now", "@ops take this", true});
    check(routeSend(makeRound({mentioned}), {"ops"}, "dv")
              .responderCommentThreads.empty());
};

auto tRouteResponderOff = test("Responder/with no responder nothing goes to it") = []
{
    auto route = routeSend(makeRound({makeCommentThread(4, {})}), {}, {});

    check(route.listeners.empty() && route.responderCommentThreads.empty());
    check(describeSendRoute(route, {}) == "nobody listening");
};

auto tFrameUserMessage =
    test("Responder/a message is one stream-json user line, text escaped") = []
{
    auto text = makeResponderMessage("dv: reply 2026 on #4 · \"quoted\"", "dv");
    auto line = frameUserMessage(text, {});

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
    responder.send({"first", {4}, {}});
    check(launcher.started.size() == 1);

    auto& process = *launcher.started.front();
    check(process.launch.workingDirectory == "/r/main");
    check(process.launch.arguments
          == makeAgentArguments(getAgentCli("claude"), dvExecutable, "dv", {}, {}));
    check(readMessageText(process.written) == "first");

    check(responder.countQueuedMessages() == 0);
    responder.takeHasQueueChanged();

    responder.send({"second", {5}, {}});
    check(launcher.started.size() == 1);
    check(process.written.find("second") == std::string::npos);
    check(responder.countQueuedMessages() == 1);
    check(responder.takeHasQueueChanged() && !responder.takeHasQueueChanged());

    writeOutput(dir, claudeInit);
    responder.poll();
    check(responder.findActivities().size() == 1);
    check(responder.findActivities().front().commentThread == 4);
    check(responder.findActivities().front().doing == "starting");

    writeOutput(dir, makeEcho("first") + claudeResult);
    responder.poll();
    check(launcher.started.size() == 1);
    check(process.written.find("second") != std::string::npos);
    check(responder.findActivities().front().commentThread == 5);
    check(responder.countQueuedMessages() == 0 && responder.takeHasQueueChanged());
    check(responder.getProcessId() == firstFakePid);

    responder.finish();
    check(process.isInputClosed && responder.isFinishing());
};

auto tResponderResultBeforeEcho =
    test("Responder/a turn's end before the message's echo doesn't answer it") = []
{
    auto dir = makeEmptyDirectory("ResponderResultBeforeEcho");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    responder.send({"second", {5}, {}});
    writeOutput(dir, claudeInit + claudeResult);
    responder.poll();

    check(responder.findActivities().front().commentThread == 4);
    check(launcher.started.front()->written.find("second") == std::string::npos);
    check(responder.countQueuedMessages() == 1);
};

auto tResponderEchoThenResult =
    test("Responder/the first turn's end after its echo answers a message") = []
{
    auto dir = makeEmptyDirectory("ResponderEchoThenResult");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    responder.send({"second", {5}, {}});
    writeOutput(dir, claudeInit + makeEcho("first"));
    responder.poll();
    check(responder.findActivities().front().commentThread == 4);

    writeOutput(dir, claudeResult);
    responder.poll();
    check(responder.findActivities().front().commentThread == 5);
    check(readMessageText(launcher.started.front()->written.substr(
              launcher.started.front()->written.find('\n') + 1))
          == "second");
};

auto tResponderSpontaneousTurn =
    test("Responder/a turn the agent starts itself between two rounds doesn't "
         "answer the second") = []
{
    const auto subagentNotification =
        std::string {
            R"({"type":"system","subtype":"task_notification","task_id":"a1","status":"completed"})"}
        + "\n";
    auto dir = makeEmptyDirectory("ResponderSpontaneousTurn");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    writeOutput(dir, claudeInit + makeEcho("first") + claudeResult);
    responder.poll();
    check(responder.findActivities().empty());

    responder.send({"second", {5}, {}});
    writeOutput(dir, subagentNotification + claudeResult);
    responder.poll();
    check(responder.findActivities().size() == 1);
    check(responder.findActivities().front().commentThread == 5);

    responder.send({"third", {6}, {}});
    check(launcher.started.front()->written.find("third") == std::string::npos);

    writeOutput(dir, makeEcho("second") + claudeResult);
    responder.poll();
    check(responder.findActivities().front().commentThread == 6);
    check(launcher.started.front()->written.find("third") != std::string::npos);
};

auto tResponderResumes =
    test("Responder/its session id is kept and the next one resumes it") = []
{
    auto dir = makeEmptyDirectory("ResponderResumes");
    auto launcher = FakeLauncher {};

    {
        auto responder = Responder {makeSettings(dir), launcher.makeStart()};
        responder.send({"first", {4}, {}});
        writeOutput(dir, claudeInit + makeEcho("first") + claudeResult);
        responder.poll();
    }

    check(launcher.started.front()->isInputClosed);
    check(eacp::Files::readFile(dir / "dv.session") == "session-1\n");

    auto responder = Responder {makeSettings(dir), launcher.makeStart()};
    responder.send({"again", {4}, {}});
    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(
              getAgentCli("claude"), dvExecutable, "dv", "session-1", {}));
};

auto tResponderFailedResume =
    test("Responder/a session that can't be resumed is forgotten, not a crash") = []
{
    auto dir = makeEmptyDirectory("ResponderFailedResume");
    writeText(dir / "dv.session", "gone\n");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    writeOutput(
        dir,
        R"({"type":"result","subtype":"error_during_execution","session_id":"gone"})"
        "\n");
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(getAgentCli("claude"), dvExecutable, "dv", {}, {}));
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

    responder.send({"first", {4}, {}});
    writeOutput(dir, claudeInit);
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2);
    check(readMessageText(launcher.started.back()->written) == "first");
    check(responder.takeNotices()
          == std::vector<std::string> {"@dv crashed (exit 1), restarted"});

    writeOutput(dir, claudeInit);
    launcher.started.back()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2 && !responder.isRunning());
    check(responder.getFailure().has_value());
    check(
        responder.getFailure()->starts_with("@dv crashed twice (exit 1), stopped"));
    check(contains(responder.takeNotices(), *responder.getFailure()));
    check(responder.findActivities().empty());

    responder.send({"ignored", {5}, {}});
    check(launcher.started.size() == 2);
};

auto tResponderIdleCrash =
    test("Responder/a crash while idle restarts it at once, ready for the next") = []
{
    auto dir = makeEmptyDirectory("ResponderIdleCrash");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    writeOutput(dir, claudeInit + makeEcho("first") + claudeResult);
    responder.poll();
    launcher.started.front()->exitCode = crashExitCode;
    responder.poll();

    check(launcher.started.size() == 2 && responder.isRunning());
    check(launcher.started.back()->written.empty());
    check(launcher.started.back()->launch.arguments
          == makeAgentArguments(
              getAgentCli("claude"), dvExecutable, "dv", "session-1", {}));
};

auto tResponderOneShot =
    test("Responder/a CLI without a stdin session runs once per message") = []
{
    auto dir = makeEmptyDirectory("ResponderOneShot");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir, "gemini"), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    responder.send({"second", {5}, {}});
    check(launcher.started.size() == 1);
    check(launcher.started.front()->launch.arguments.back() == "first");
    check(launcher.started.front()->written.empty());

    launcher.started.front()->exitCode = 0;
    responder.poll();
    check(launcher.started.size() == 2);
    check(launcher.started.back()->launch.arguments.back() == "second");
    check(responder.takeNotices().empty());
};

auto tResponderLog =
    test("Responder/what dv did goes to its log, the stream to its own file") = []
{
    auto dir = makeEmptyDirectory("ResponderLog");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    writeOutput(dir, claudeInit);
    responder.poll();

    auto log = eacp::Files::readFile(dir / "dv.log");
    check(log.find("dv: sending #4: first") != std::string::npos);
    check(log.find("dv: started pid 1000: claude") != std::string::npos);
    check(log.find(claudeInit) == std::string::npos);
    check(launcher.started.front()->launch.logPath == (dir / "dv.log").str());
    check(launcher.started.front()->launch.outputPath == (dir / "dv.jsonl").str());
};

auto tResponderReadsOutputByOffset =
    test("Responder/reads its stream from where it began, a line at a time") = []
{
    auto dir = makeEmptyDirectory("ResponderReadsOutputByOffset");
    writeOutput(dir, claudeResult);
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    responder.send({"second", {5}, {}});
    responder.poll();
    check(responder.findActivities().front().commentThread == 4);

    auto half = claudeInit.size() / 2;
    writeOutput(dir, claudeInit.substr(0, half));
    responder.poll();
    check(responder.findActivities().front().doing == "starting");

    writeOutput(dir, claudeInit.substr(half) + makeEcho("first") + claudeResult);
    responder.poll();
    check(responder.findActivities().front().commentThread == 5);
};

auto tResponderFinishes =
    test("Responder/finishing hands it the queue and closes its stdin, and its "
         "exit is no crash") = []
{
    auto dir = makeEmptyDirectory("ResponderFinishes");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send({"first", {4}, {}});
    responder.send({"second", {5}, {}});
    responder.finish();

    auto& process = *launcher.started.front();
    check(process.isInputClosed);
    check(process.written.find("second") != std::string::npos);
    check(responder.isRunning() && responder.isFinishing());
    check(responder.findActivities().empty());

    process.exitCode = crashExitCode;
    responder.poll();
    check(!responder.isRunning() && launcher.started.size() == 1);
    check(responder.takeNotices().empty() && !responder.getFailure());
};

#if !defined(_WIN32)
auto tAgentProcessOutlivesItsOwner =
    test("Responder/a closed agent process is not signalled and finishes on its "
         "own, in its own session") = []
{
    constexpr auto finishDeadline = std::chrono::seconds {5};
    constexpr auto pollInterval = std::chrono::milliseconds {20};

    auto dir = makeEmptyDirectory("AgentProcessOutlivesItsOwner");
    auto outputPath = dir / "out.jsonl";
    auto launch =
        AgentLaunch {{"/bin/sh", "-c", "cat >/dev/null; sleep 0.3; echo finished"},
                     dir.str(),
                     outputPath.str(),
                     (dir / "err.log").str()};

    auto process = startPipedAgentProcess(launch);
    check(process != nullptr);

    auto pid = static_cast<pid_t>(process->getProcessId());
    check(getsid(pid) == pid);
    check(process->writeInput("input\n"));
    process->closeInput();
    process.reset();

    auto deadline = std::chrono::steady_clock::now() + finishDeadline;

    while (eacp::Files::readFile(outputPath).empty()
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(pollInterval);
    }

    auto status = 0;
    waitpid(pid, &status, 0);
    check(eacp::Files::readFile(outputPath) == "finished\n");
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0);
};
#endif

auto tResponderTypedMessage =
    test("Responder/a typed message is sent as typed, about no thread") = []
{
    auto typed = makeTypedResponderMessage("what are you doing? \"now\"");

    check(typed.text == "what are you doing? \"now\"");
    check(typed.commentThreads.empty());
    check(describeMessageOrigin(typed) == "typed");
    check(describeMessageOrigin({"round", {4, 5}, {}}) == "#4 #5");
    check(readMessageText(frameUserMessage(typed.text, {})) == typed.text);
};

auto tResponderTypedMessageStarts =
    test("Responder/a typed message starts the agent, or waits behind a turn") = []
{
    auto dir = makeEmptyDirectory("ResponderTypedMessage");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};

    responder.send(makeTypedResponderMessage("hello"));
    check(launcher.started.size() == 1);

    auto& process = *launcher.started.front();
    check(readMessageText(process.written) == "hello");
    check(responder.findActivities().empty());

    responder.send(makeTypedResponderMessage("and then"));
    check(process.written.find("and then") == std::string::npos);

    writeOutput(dir, claudeInit + makeEcho("hello") + claudeResult);
    responder.poll();
    check(process.written.find("and then") != std::string::npos);

    auto log = eacp::Files::readFile(dir / "dv.log");
    check(log.find("dv: sending typed: hello") != std::string::npos);
};

namespace
{
constexpr auto pastedPath = ".diffview/images/chat-1-1.png";
constexpr auto pastedPng = "\x89PNG bytes";
} // namespace

auto tResponderImageBlocks =
    test("Responder/a message's images follow its text as base64 PNG blocks") = []
{
    auto line = frameUserMessage("look", {pastedPng});
    auto event = Miro::Json::getParsedValue(line);
    const auto& content = getField(getField(event, "message"), "content");

    check(content.isArray() && content.asArray().size() == 2);
    check(readMessageText(line) == "look");

    const auto& image = content.asArray().back();
    const auto& source = getField(image, "source");
    check(getString(image, "type") == "image");
    check(getString(source, "type") == "base64");
    check(getString(source, "media_type") == "image/png");
    check(getString(source, "data") == eacp::Base64::encode(pastedPng));
};

auto tResponderTypedMessageImages = test(
    "Responder/a typed message sends the images it links, read when it goes") = []
{
    auto dir = makeEmptyDirectory("ResponderTypedMessageImages");
    auto settings = makeSettings(dir);
    settings.workingDirectory = dir.str();
    writeText(dir / pastedPath, pastedPng);
    auto launcher = FakeLauncher {};
    auto responder = Responder {settings, launcher.makeStart()};

    auto typed = std::string {"see ![image]("} + pastedPath + ")";
    auto message = makeTypedResponderMessage(typed);
    check(message.imagePaths == std::vector<std::string> {pastedPath});

    responder.send(message);

    const auto& written = launcher.started.front()->written;
    check(readMessageText(written) == typed);
    check(written.find(eacp::Base64::encode(pastedPng)) != std::string::npos);

    writeOutput(dir, claudeInit + makeEcho(typed) + claudeResult);
    responder.poll();
    check(responder.findActivities().empty());
};

namespace
{
const auto responderInbox = InboxAddress {"/tmp/cc-socks/1000.sock", "token"};

struct FakeInbox
{
    std::vector<std::string> posted;
    bool isListening = true;
};

ResponderSettings makeSettingsWithInbox(const eacp::FilePath& dir, FakeInbox& inbox)
{
    auto settings = makeSettings(dir);
    settings.findInbox = [](int agentPid) -> std::optional<InboxAddress>
    {
        if (agentPid != firstFakePid)
            return std::nullopt;

        return responderInbox;
    };
    settings.postToInbox =
        [&inbox](const InboxAddress& address, const std::string& text)
    {
        if (!inbox.isListening || address.socketPath != responderInbox.socketPath)
            return false;

        inbox.posted.push_back(text);
        return true;
    };
    return settings;
}
} // namespace

auto tResponderPostsMidTurn =
    test("Responder/mid-turn a message goes to its inbox, not behind the turn") = []
{
    auto dir = makeEmptyDirectory("ResponderPostsMidTurn");
    auto launcher = FakeLauncher {};
    auto inbox = FakeInbox {};
    auto responder =
        Responder {makeSettingsWithInbox(dir, inbox), launcher.makeStart()};

    responder.send({"first", {4}});
    responder.send({"second", {5}});
    check(inbox.posted.empty());
    check(responder.countQueuedMessages() == 1);

    writeOutput(dir, claudeInit + makeEcho("first"));
    responder.poll();
    check(inbox.posted == std::vector<std::string> {"second"});
    check(responder.countQueuedMessages() == 0);
    check(responder.findActivities().size() == 2);

    responder.send(makeTypedResponderMessage("third"));
    check(inbox.posted.back() == "third");

    auto& process = *launcher.started.front();
    check(process.written.find("second") == std::string::npos);
    check(process.written.find("third") == std::string::npos);

    writeOutput(dir, claudeResult);
    responder.poll();
    check(responder.findActivities().empty());

    auto log = eacp::Files::readFile(dir / "dv.log");
    check(log.find("dv: posted mid-turn to its inbox #5: second")
          != std::string::npos);
};

auto tResponderIdleUsesStdin =
    test("Responder/with no turn in flight a message still goes to stdin") = []
{
    auto dir = makeEmptyDirectory("ResponderIdleUsesStdin");
    auto launcher = FakeLauncher {};
    auto inbox = FakeInbox {};
    auto responder =
        Responder {makeSettingsWithInbox(dir, inbox), launcher.makeStart()};

    responder.send({"first", {4}});
    writeOutput(dir, claudeInit + makeEcho("first") + claudeResult);
    responder.poll();
    responder.send({"second", {5}});

    check(inbox.posted.empty());
    check(launcher.started.front()->written.find("second") != std::string::npos);
};

auto tResponderInboxFallsBackToStdin =
    test("Responder/without a reachable inbox a message waits for stdin") = []
{
    auto dir = makeEmptyDirectory("ResponderInboxFallsBack");
    auto launcher = FakeLauncher {};
    auto inbox = FakeInbox {};
    inbox.isListening = false;
    auto responder =
        Responder {makeSettingsWithInbox(dir, inbox), launcher.makeStart()};

    responder.send({"first", {4}});
    writeOutput(dir, claudeInit + makeEcho("first"));
    responder.poll();
    responder.send({"second", {5}});
    check(responder.countQueuedMessages() == 1);

    writeOutput(dir, claudeResult);
    responder.poll();
    check(responder.countQueuedMessages() == 0);
    check(launcher.started.front()->written.find("second") != std::string::npos);
};

auto tResponderPostedEchoIsNotStdins =
    test("Responder/a posted message's echo doesn't stand for the stdin one") = []
{
    auto dir = makeEmptyDirectory("ResponderPostedEcho");
    auto launcher = FakeLauncher {};
    auto responder = Responder {makeSettings(dir), launcher.makeStart()};
    const auto peerEcho = std::string {
        R"({"type":"user","message":{"role":"user","content":"first"},"isSynthetic":true,"origin":{"kind":"peer","from":"unknown"}})"};

    responder.send({"first", {4}});
    responder.send({"second", {5}});
    writeOutput(dir, claudeInit + peerEcho + "\n" + claudeResult);
    responder.poll();
    check(responder.findActivities().front().commentThread == 4);
};

auto tRouteTypedToResponder =
    test("Responder/a typed message without @name, or @ the responder, is its") = []
{
    auto listening = std::vector<ListeningAgent> {{"ops", responderInbox}};

    check(routeTypedMessage("fix it", "dv", listening).destination
          == TypedMessageDestination::responder);
    check(routeTypedMessage("@dv fix it", "dv", listening).destination
          == TypedMessageDestination::responder);
    check(routeTypedMessage("@ops", "dv", listening).destination
          == TypedMessageDestination::responder);
    check(routeTypedMessage("email@ops fix", "dv", listening).destination
          == TypedMessageDestination::responder);
};

auto tRouteTypedToAgent =
    test("Responder/a typed @name goes to that listening agent's inbox") = []
{
    auto listening =
        std::vector<ListeningAgent> {{"ops", responderInbox}, {"quiet", {}}};
    auto toOps =
        routeTypedMessage("@ops rebase on main\nthen test", "dv", listening);

    check(toOps.destination == TypedMessageDestination::agentInbox);
    check(toOps.agent == "ops");
    check(toOps.text == "rebase on main\nthen test");
    check(toOps.inbox.socketPath == responderInbox.socketPath);

    auto toQuiet = routeTypedMessage("@quiet hi", "dv", listening);
    check(toQuiet.destination == TypedMessageDestination::agentWithoutInbox);

    auto toNobody = routeTypedMessage("@ghost hi", "dv", listening);
    check(toNobody.destination == TypedMessageDestination::unknownAgent);
    check(toNobody.agent == "ghost");
};
