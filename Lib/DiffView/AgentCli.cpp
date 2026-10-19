#include "AgentCli.h"

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

bool canResume(const AgentCli& cli)
{
    return !cli.resumeFlag.empty();
}

bool hasStdinSession(const AgentCli& cli)
{
    return !cli.stdinSessionArguments.empty();
}

std::vector<std::string> makeAgentArguments(const AgentCli& cli,
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

    if (hasStdinSession(cli))
        return arguments;

    if (!cli.promptFlag.empty())
        arguments.push_back(cli.promptFlag);

    arguments.push_back(prompt);
    return arguments;
}
} // namespace diffview
