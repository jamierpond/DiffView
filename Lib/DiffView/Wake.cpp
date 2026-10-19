#include "Wake.h"
#include "Activity.h"
#include "Text.h"

#include <algorithm>
#include <charconv>

namespace diffview
{
namespace
{
constexpr auto sessionKey = std::string_view {"session"};
constexpr auto claudePidKey = std::string_view {"claude-pid"};
constexpr auto worktreeKey = std::string_view {"worktree"};

std::optional<std::pair<std::string, std::string>> splitAt(const std::string& line,
                                                           char separator)
{
    auto at = line.find(separator);

    if (at == std::string::npos)
        return std::nullopt;

    return std::make_pair(trim(line.substr(0, at)), trim(line.substr(at + 1)));
}

std::optional<WakeMode> parseWakeMode(const std::string& text)
{
    static const auto modes = std::map<std::string, WakeMode> {
        {"resume", WakeMode::resume},
        {"fresh", WakeMode::fresh},
        {"off", WakeMode::off},
    };

    if (auto found = modes.find(text); found != modes.end())
        return found->second;

    return std::nullopt;
}

int parseNumber(const std::string& text)
{
    auto value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

std::map<std::string, std::vector<int>> findOwnersToWake(const ReviewFile& round)
{
    auto owners = std::map<std::string, std::vector<int>> {};

    for (const auto& comment: round.comments)
    {
        auto owner = findOwner(comment);
        auto thread = ReviewFile {round.title, {}, {}, {comment}, round.reply};

        if (!owner.empty() && routeRound(thread, owner, {owner}).wakes())
            owners[owner].push_back(comment.id);
    }

    return owners;
}

WakeDecision decideWake(const std::string& agent,
                        const std::vector<int>& threads,
                        const std::map<std::string, KnownSession>& sessions,
                        WakeMode mode)
{
    if (mode == WakeMode::off)
        return {WakeKind::off, agent, {}, threads};

    if (mode == WakeMode::fresh)
        return {WakeKind::fresh, agent, {}, threads};

    auto known = sessions.find(agent);

    if (known == sessions.end() || known->second.session.sessionId.empty())
        return {WakeKind::freshWithoutSession, agent, {}, threads};

    if (known->second.isClaudeAlive)
        return {WakeKind::freshBesideOpenSession, agent, {}, threads};

    return {WakeKind::resumed, agent, known->second.session.sessionId, threads};
}

std::string describeWake(const WakeDecision& decision)
{
    auto name = "@" + decision.agent;

    switch (decision.kind)
    {
        case WakeKind::listening:
            return name + " listening";
        case WakeKind::resumed:
            return "woke " + name + " (resumed)";
        case WakeKind::freshWithoutSession:
            return "woke " + name + " (fresh: no session known)";
        case WakeKind::freshBesideOpenSession:
            return "woke " + name + " (fresh: its session is open)";
        case WakeKind::fresh:
            return "woke " + name + " (fresh)";
        case WakeKind::off:
            return "nobody listening, wake off";
    }

    return {};
}
} // namespace

WakeConfig parseWakeConfig(const std::string& text, WakeConfig config)
{
    for (const auto& line: splitLines(text))
    {
        auto setting = splitAt(line, '=');

        if (!setting || line.starts_with("#"))
            continue;

        const auto& [key, value] = *setting;

        if (key == "wake")
            config.mode = parseWakeMode(value).value_or(config.mode);
        else if (key == "wakeCommand" && !value.empty())
            config.command = value;
    }

    return config;
}

std::string formatAgentSession(const AgentSession& session)
{
    return std::string {sessionKey} + " " + session.sessionId + "\n"
           + std::string {claudePidKey} + " " + std::to_string(session.claudePid)
           + "\n" + std::string {worktreeKey} + " " + session.worktree + "\n";
}

AgentSession parseAgentSession(const std::string& text)
{
    auto session = AgentSession {};

    for (const auto& line: splitLines(text))
    {
        auto field = splitAt(line, ' ');

        if (!field)
            continue;

        const auto& [key, value] = *field;

        if (key == sessionKey)
            session.sessionId = value;
        else if (key == claudePidKey)
            session.claudePid = parseNumber(value);
        else if (key == worktreeKey)
            session.worktree = value;
    }

    return session;
}

std::vector<WakeDecision>
    decideWakes(const ReviewFile& round,
                const std::vector<std::string>& listening,
                const std::map<std::string, KnownSession>& sessions,
                WakeMode mode)
{
    auto decisions = std::vector<WakeDecision> {};

    for (const auto& agent: listening)
    {
        if (routeRound(round, agent, listening).wakes())
            decisions.push_back({WakeKind::listening, agent, {}, {}});
    }

    if (!listening.empty())
        return decisions;

    for (const auto& [owner, threads]: findOwnersToWake(round))
        decisions.push_back(decideWake(owner, threads, sessions, mode));

    return decisions;
}

std::string describeWakes(const std::vector<WakeDecision>& decisions)
{
    if (decisions.empty())
        return "nobody listening";

    auto parts = std::vector<std::string> {};

    for (const auto& decision: decisions)
    {
        auto part = describeWake(decision);

        if (std::find(parts.begin(), parts.end(), part) == parts.end())
            parts.push_back(part);
    }

    auto text = std::string {};

    for (const auto& part: parts)
        text += (text.empty() ? "" : ", ") + part;

    return text;
}

std::string makeWakeNotice(const ReviewFile& round,
                           const std::string& agent,
                           const std::string& path)
{
    if (round.reply)
        return replyNotice(round.title, round, agent, {}, path);

    return roundNotice(
        round.title, routeRound(round, agent, {agent}), agent, {}, path);
}

std::string makeWakePrompt(const std::string& notice, const std::string& agent)
{
    return notice + "\nHandle it with dv comments/show/reply/resolve as " + agent
           + ", then exit.";
}

WakeLaunch makeWakeLaunch(const WakeDecision& decision,
                          const WakeConfig& config,
                          const std::string& prompt,
                          const std::string& worktree,
                          const std::string& logPath)
{
    auto threads = std::string {};

    for (auto thread: decision.threads)
        threads += (threads.empty() ? "" : " ") + std::to_string(thread);

    auto claude = config.command + " -p --output-format stream-json --verbose";
    auto fresh = claude + " \"$1\"";
    auto run = fresh;

    if (decision.kind == WakeKind::resumed)
        run = claude + " --resume \"$4\" \"$1\" || " + fresh;

    auto script = "echo " + std::string {wakeHeaderWord}
                  + " pid $$ agent \"$2\" threads $3; " + run + "; echo "
                  + std::string {wakeExitWord} + " $?";
    auto launch = WakeLaunch {
        worktree,
        logPath,
        {"/bin/sh", "-c", script, "dv-wake", prompt, decision.agent, threads}};

    if (decision.kind == WakeKind::resumed)
        launch.arguments.push_back(decision.sessionId);

    return launch;
}
} // namespace diffview
