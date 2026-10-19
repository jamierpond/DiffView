#pragma once

#include "Activity.h"
#include "AgentCli.h"
#include "Review.h"

#include <eacp/Core/Utils/FilePath.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
constexpr auto responderDirectory = "responder";
constexpr auto configFileName = "config";
constexpr auto responderOutputExtension = ".jsonl";
constexpr auto defaultResponderName = std::string_view {"dv"};

struct ResponderConfig
{
    bool isEnabled = true;
    std::string agentCli {defaultAgentCliName};
    std::string name {defaultResponderName};
};

// "key = value" lines, as git's config reads; later files override earlier.
ResponderConfig parseResponderConfig(const std::string& text,
                                     ResponderConfig config);

struct SendRoute
{
    std::vector<std::string> listeners;
    std::vector<int> responderCommentThreads;
};

// Listening agents hear what `dv monitor --as` gives them. The responder
// takes each thread none of them will: unowned with nobody listening, owned
// by someone who isn't, or its own. An empty responder takes nothing.
SendRoute routeSend(const ReviewFile& round,
                    const std::vector<std::string>& listening,
                    const std::string& responder);

std::string describeSendRoute(const SendRoute& route, const std::string& responder);

std::string makeResponderMessage(const std::string& notice,
                                 const std::string& responder);

// One stream-json user message: a JSON line.
std::string frameUserMessage(const std::string& text);

struct AgentLaunch
{
    std::vector<std::string> arguments;
    std::string workingDirectory;
    // Gets its stdout.
    std::string outputPath;
    // Gets its stderr.
    std::string logPath;
};

class AgentProcess
{
public:
    virtual ~AgentProcess() = default;

    virtual int getProcessId() const = 0;
    virtual bool writeInput(std::string_view text) = 0;
    virtual std::optional<int> findExitCode() = 0;
    // A stream-json CLI finishes its turn and ends on EOF.
    virtual void closeInput() = 0;
};

// Nothing when it couldn't be started.
using StartAgentProcess =
    std::function<std::unique_ptr<AgentProcess>(const AgentLaunch&)>;

struct ResponderSettings
{
    std::string name;
    AgentCli cli;
    std::string workingDirectory;
    eacp::FilePath logPath;
    // Its stream-json stdout, read back by byte offset: the CLI keeps writing
    // it after dv has gone.
    eacp::FilePath outputPath;
    eacp::FilePath sessionPath;
    // What the CLI's hooks run to reach dv.
    std::string dvExecutable;
};

struct ResponderMessage
{
    std::string text;
    std::vector<int> commentThreads;
};

// What someone typed into dv's agent pane: sent as typed, about no thread.
ResponderMessage makeTypedResponderMessage(const std::string& text);

// "#4 #5" for a round's threads, "typed" for a typed message.
std::string describeMessageOrigin(const ResponderMessage& message);

// dv's own agent: one CLI process, started on the first message, fed the
// rest on its stdin one at a time, so it keeps its context. Its session id
// is kept to resume on the next start. Restarted once after a crash; a
// second crash stops it. It is never killed: finish() hands it the rest and
// closes its stdin, and it ends on its own, with or without dv.
class Responder
{
public:
    Responder(ResponderSettings settings, StartAgentProcess startProcess);
    ~Responder();

    Responder(const Responder&) = delete;
    Responder& operator=(const Responder&) = delete;

    void send(ResponderMessage message);
    void poll();
    void finish();

    const std::string& getName() const;
    bool isRunning() const;
    bool isFinishing() const;
    std::optional<int> getProcessId() const;
    std::vector<AgentActivity> findActivities() const;
    const std::optional<std::string>& getFailure() const;
    std::vector<std::string> takeNotices();
    // Waiting behind the message in flight.
    int countQueuedMessages() const;
    // Whether the queue changed since the last call.
    bool takeHasQueueChanged();

private:
    void sendNext();
    bool start(const std::string& prompt);
    void readOutput();
    void readLine(std::string_view line);
    void finishTurn();
    void handleExit(int exitCode);
    void recordSessionId(const std::string& sessionId);
    void forgetSessionId();
    void log(const std::string& text);

    ResponderSettings settings;
    StartAgentProcess startProcess;
    std::unique_ptr<AgentProcess> process;
    std::deque<ResponderMessage> queue;
    bool hasQueueChanged = false;
    std::optional<ResponderMessage> current;
    // Its echo came back: the next end of a turn is its answer's.
    bool isCurrentPickedUp = false;
    std::string sessionId;
    bool isResuming = false;
    bool hasReportedActivity = false;
    std::string doing;
    std::string partialLine;
    std::uint64_t outputOffset = 0;
    bool isInputClosed = false;
    int crashes = 0;
    std::optional<std::string> failure;
    std::vector<std::string> notices;
};
} // namespace diffview
