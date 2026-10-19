#pragma once

#include "Activity.h"
#include "AgentCli.h"
#include "Review.h"

#include <eacp/Core/Utils/FilePath.h>

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
    // Gets its stderr.
    std::string logPath;
};

class AgentProcess
{
public:
    virtual ~AgentProcess() = default;

    virtual int getProcessId() const = 0;
    virtual bool writeInput(std::string_view text) = 0;
    virtual std::string readAvailableOutput() = 0;
    virtual std::optional<int> findExitCode() = 0;
    // Closes its stdin and terminates it and everything it started.
    virtual void stop() = 0;
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
    eacp::FilePath sessionPath;
};

struct ResponderMessage
{
    std::string text;
    std::vector<int> commentThreads;
};

// dv's own agent: one CLI process, started on the first message, fed the
// rest on its stdin one at a time, so it keeps its context. Its session id
// is kept to resume on the next start. Restarted once after a crash; a
// second crash stops it.
class Responder
{
public:
    Responder(ResponderSettings settings, StartAgentProcess startProcess);
    ~Responder();

    Responder(const Responder&) = delete;
    Responder& operator=(const Responder&) = delete;

    void send(ResponderMessage message);
    void poll();
    void stop();

    const std::string& getName() const;
    bool isRunning() const;
    std::optional<int> getProcessId() const;
    std::vector<AgentActivity> findActivities() const;
    const std::optional<std::string>& getFailure() const;
    std::vector<std::string> takeNotices();

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
    std::optional<ResponderMessage> current;
    std::string sessionId;
    bool isResuming = false;
    bool hasReportedActivity = false;
    std::string doing;
    std::string partialLine;
    int crashes = 0;
    std::optional<std::string> failure;
    std::vector<std::string> notices;
};
} // namespace diffview
