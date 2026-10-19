#include "Responder.h"
#include "Store.h"
#include "StreamEvents.h"
#include "Text.h"

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <map>

namespace diffview
{
namespace
{
constexpr auto crashesBeforeStopping = 2;
constexpr auto exitCodeForFailedStart = 127;

std::optional<std::pair<std::string, std::string>>
    splitSetting(const std::string& line)
{
    auto at = line.find('=');

    if (at == std::string::npos || line.starts_with("#"))
        return std::nullopt;

    return std::make_pair(trim(line.substr(0, at)), trim(line.substr(at + 1)));
}

std::optional<bool> parseSwitch(const std::string& text)
{
    static const auto switches = std::map<std::string, bool> {
        {"on", true},
        {"off", false},
    };

    if (auto found = switches.find(text); found != switches.end())
        return found->second;

    return std::nullopt;
}

bool contains(const std::vector<std::string>& names, const std::string& name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

ReviewFile makeThreadRound(const ReviewFile& round, const Annotation& comment)
{
    auto thread = round;
    thread.comments = {comment};
    return thread;
}

std::string joinThreads(const std::vector<int>& threads)
{
    auto text = std::string {};

    for (auto thread: threads)
        text += (text.empty() ? "#" : " #") + std::to_string(thread);

    return text;
}

std::string joinArguments(const std::vector<std::string>& arguments)
{
    auto text = std::string {};

    for (const auto& argument: arguments)
        text += (text.empty() ? "" : " ") + argument;

    return text;
}
} // namespace

ResponderConfig parseResponderConfig(const std::string& text, ResponderConfig config)
{
    for (const auto& line: splitLines(text))
    {
        auto setting = splitSetting(line);

        if (!setting)
            continue;

        const auto& [key, value] = *setting;

        if (key == "responder")
            config.isEnabled = parseSwitch(value).value_or(config.isEnabled);
        else if (key == "agent" && isKnownAgentCli(value))
            config.agentCli = value;
        else if (key == "responderName" && isAgentName(value) && value != "you")
            config.name = value;
    }

    return config;
}

SendRoute routeSend(const ReviewFile& round,
                    const std::vector<std::string>& listening,
                    const std::string& responder)
{
    auto route = SendRoute {};
    auto external = std::vector<std::string> {};

    for (const auto& agent: listening)
    {
        if (agent != responder)
            external.push_back(agent);
    }

    auto everyone = external;

    if (!responder.empty())
        everyone.push_back(responder);

    for (const auto& agent: external)
    {
        if (routeRound(round, agent, everyone).wakes())
            route.listeners.push_back(agent);
    }

    if (responder.empty())
        return route;

    for (const auto& comment: round.comments)
    {
        auto thread = makeThreadRound(round, comment);
        auto isTaken =
            std::any_of(external.begin(),
                        external.end(),
                        [&](const std::string& agent)
                        { return routeRound(thread, agent, everyone).wakes(); });

        if (!isTaken && routeRound(thread, responder, everyone).wakes())
            route.responderThreads.push_back(comment.id);
    }

    return route;
}

std::string describeSendRoute(const SendRoute& route, const std::string& responder)
{
    auto parts = std::vector<std::string> {};

    for (const auto& agent: route.listeners)
        parts.push_back("@" + agent + " listening");

    if (!route.responderThreads.empty() && !contains(route.listeners, responder))
        parts.push_back("@" + responder);

    if (parts.empty())
        return "nobody listening";

    auto text = std::string {};

    for (const auto& part: parts)
        text += (text.empty() ? "" : ", ") + part;

    return text;
}

std::string makeResponderMessage(const std::string& notice,
                                 const std::string& responder)
{
    return notice + "\nHandle it with dv comments/show/reply/resolve as " + responder
           + ". dv sends you the next one itself: don't run dv monitor.";
}

std::string frameUserMessage(const std::string& text)
{
    using Miro::Json::Array;
    using Miro::Json::Object;
    using Miro::Json::Value;

    auto block = Object {{"type", Value {"text"}}, {"text", Value {text}}};
    auto message = Object {{"role", Value {"user"}},
                           {"content", Value {Array {Value {block}}}}};
    auto event = Object {{"type", Value {"user"}}, {"message", Value {message}}};

    return Miro::Json::print(Value {event}) + "\n";
}

Responder::Responder(ResponderSettings settingsToUse, StartAgentProcess startToUse)
    : settings(std::move(settingsToUse))
    , startProcess(std::move(startToUse))
{
    if (canResume(settings.cli))
        sessionId = trim(eacp::Files::readFile(settings.sessionPath));
}

Responder::~Responder()
{
    stop();
}

void Responder::send(ResponderMessage message)
{
    if (failure)
        return;

    queue.push_back(std::move(message));
    sendNext();
}

void Responder::poll()
{
    if (!process)
        return;

    readOutput();

    auto exitCode = process->findExitCode();

    if (!exitCode)
        return;

    readOutput();
    process.reset();
    handleExit(*exitCode);
}

void Responder::stop()
{
    queue.clear();
    current.reset();

    if (!process)
        return;

    process->stop();
    process.reset();
    log("dv: stopped");
}

const std::string& Responder::getName() const
{
    return settings.name;
}

bool Responder::isRunning() const
{
    return process != nullptr;
}

std::optional<int> Responder::getProcessId() const
{
    if (!process)
        return std::nullopt;

    return process->getProcessId();
}

std::vector<AgentActivity> Responder::findActivities() const
{
    auto activities = std::vector<AgentActivity> {};

    if (!current)
        return activities;

    for (auto thread: current->threads)
        activities.push_back({settings.name, thread, doing});

    return activities;
}

const std::optional<std::string>& Responder::getFailure() const
{
    return failure;
}

std::vector<std::string> Responder::takeNotices()
{
    return std::exchange(notices, {});
}

void Responder::sendNext()
{
    if (failure || current || queue.empty())
        return;

    current = std::move(queue.front());
    queue.pop_front();
    doing = "starting";
    log("dv: sending " + joinThreads(current->threads) + ": " + current->text);

    if (!hasStdinSession(settings.cli))
    {
        start(current->text);
        return;
    }

    if (!process && !start({}))
        return;

    process->writeInput(frameUserMessage(current->text));
}

bool Responder::start(const std::string& prompt)
{
    auto arguments = makeAgentArguments(settings.cli, sessionId, prompt);
    isResuming = !sessionId.empty() && canResume(settings.cli);
    hasReportedActivity = false;
    partialLine.clear();

    eacp::Files::createDirectories(settings.logPath.parentDirectory());
    process =
        startProcess({arguments, settings.workingDirectory, settings.logPath.str()});

    if (!process)
    {
        log("dv: couldn't start " + joinArguments(arguments));
        handleExit(exitCodeForFailedStart);
        return false;
    }

    if (!hasStdinSession(settings.cli))
        arguments.pop_back();

    log("dv: started pid " + std::to_string(process->getProcessId()) + ": "
        + joinArguments(arguments));
    return true;
}

void Responder::readOutput()
{
    auto text = process->readAvailableOutput();

    if (text.empty())
        return;

    eacp::Files::appendFile(settings.logPath, text);
    partialLine += text;
    auto start = std::size_t {0};

    for (auto end = partialLine.find('\n'); end != std::string::npos;
         end = partialLine.find('\n', start))
    {
        readLine(std::string_view {partialLine}.substr(start, end - start));
        start = end + 1;
    }

    partialLine.erase(0, start);
}

// A resume that fails ends with a result line before anything starts: not
// the end of a turn.
void Responder::readLine(std::string_view line)
{
    auto said = getString(Miro::Json::getParsedValue(line), "session_id");

    if (!said.empty() && said != sessionId && canResume(settings.cli))
        recordSessionId(said);

    if (auto summary =
            settings.cli.summariseStreamLine(line, settings.workingDirectory))
    {
        doing = *summary;
        hasReportedActivity = true;
    }

    if (hasReportedActivity && settings.cli.isEndOfTurn(line))
        finishTurn();
}

void Responder::finishTurn()
{
    current.reset();
    doing.clear();
    sendNext();
}

void Responder::handleExit(int exitCode)
{
    auto exit = "exit " + std::to_string(exitCode);
    log("dv: " + exit);
    process.reset();

    if (!hasStdinSession(settings.cli) && exitCode == 0)
    {
        finishTurn();
        return;
    }

    if (current)
        queue.push_front(std::move(*current));

    current.reset();
    doing.clear();

    if (isResuming && !hasReportedActivity)
    {
        log("dv: couldn't resume " + sessionId + ", starting a new session");
        forgetSessionId();
        sendNext();
        return;
    }

    auto name = "@" + settings.name;

    if (++crashes >= crashesBeforeStopping)
    {
        failure = name + " crashed twice (" + exit + "), stopped · see "
                  + settings.logPath.str();
        log("dv: " + *failure);
        notices.push_back(*failure);
        queue.clear();
        return;
    }

    notices.push_back(name + " crashed (" + exit + "), restarted");

    if (queue.empty() && hasStdinSession(settings.cli))
        start({});

    sendNext();
}

void Responder::recordSessionId(const std::string& id)
{
    sessionId = id;
    writeText(settings.sessionPath, id + "\n");
}

void Responder::forgetSessionId()
{
    sessionId.clear();
    eacp::Files::removeFile(settings.sessionPath);
}

void Responder::log(const std::string& text)
{
    eacp::Files::createDirectories(settings.logPath.parentDirectory());
    eacp::Files::appendFile(settings.logPath, text + "\n");
}
} // namespace diffview
