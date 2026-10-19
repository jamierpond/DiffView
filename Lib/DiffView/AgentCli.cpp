#include "AgentCli.h"
#include "Hooks.h"

#include <algorithm>

namespace diffview
{
AgentCli makeClaudeCodeCli();
AgentCli makeGeminiCli();

const std::vector<AgentCli>& getAgentClis()
{
    static const auto clis = std::vector<AgentCli> {
        makeClaudeCodeCli(),
        makeGeminiCli(),
    };

    return clis;
}

bool isKnownAgentCli(std::string_view name)
{
    for (const auto& cli: getAgentClis())
    {
        if (cli.name == name)
            return true;
    }

    return false;
}

const AgentCli& getAgentCli(std::string_view name)
{
    for (const auto& cli: getAgentClis())
    {
        if (cli.name == name)
            return cli;
    }

    return getAgentCli(defaultAgentCliName);
}

namespace
{
constexpr auto maxAncestorDepth = 64;
constexpr auto initPid = 1;

std::string getBaseName(const std::string& path)
{
    auto slash = path.rfind('/');

    if (slash == std::string::npos)
        return path;

    return path.substr(slash + 1);
}

bool isAgentCliProcess(const ProcessInfo& process)
{
    constexpr auto namesToCheck = std::size_t {2};
    auto count = std::min(namesToCheck, process.arguments.size());

    for (std::size_t i = 0; i < count; ++i)
    {
        auto name = getBaseName(process.arguments[i]);

        for (const auto& cli: getAgentClis())
        {
            if (cli.processName == name)
                return true;
        }
    }

    return false;
}
} // namespace

std::optional<int> findAgentCliAncestor(int pid, const ReadProcessInfo& readProcess)
{
    for (auto depth = 0; depth < maxAncestorDepth && pid > initPid; ++depth)
    {
        auto process = readProcess(pid);

        if (!process)
            return std::nullopt;

        if (isAgentCliProcess(*process))
            return pid;

        pid = process->parentPid;
    }

    return std::nullopt;
}

bool canResume(const AgentCli& cli)
{
    return !cli.resumeFlag.empty();
}

bool hasStdinSession(const AgentCli& cli)
{
    return !cli.stdinSessionArguments.empty();
}

std::vector<std::string> makeAgentArguments(const AgentCli& cli,
                                            const std::string& dvExecutable,
                                            const std::string& sessionId,
                                            const std::string& prompt)
{
    auto arguments = cli.unattendedCommand;
    auto append = [&](const std::vector<std::string>& more)
    { arguments.insert(arguments.end(), more.begin(), more.end()); };

    append(cli.headlessArguments);
    append(cli.stdinSessionArguments);

    if (!sessionId.empty() && canResume(cli))
        append({cli.resumeFlag, sessionId});

    if (!cli.settingsFlag.empty())
        append({cli.settingsFlag, makeBackgroundAgentHookSettings(dvExecutable)});

    if (!cli.systemPromptFlag.empty())
        append({cli.systemPromptFlag, std::string {responderStandingOrders}});

    if (hasStdinSession(cli))
        return arguments;

    if (!cli.promptFlag.empty())
        arguments.push_back(cli.promptFlag);

    arguments.push_back(prompt);
    return arguments;
}
} // namespace diffview
