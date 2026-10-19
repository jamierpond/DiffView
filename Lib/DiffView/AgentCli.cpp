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

std::optional<AgentCli> detectAgentCli(
    const std::function<std::string(const std::string&)>& readVariable)
{
    for (const auto& cli: getAgentClis())
    {
        if (!readVariable(cli.detectionVariable).empty())
            return cli;
    }

    return std::nullopt;
}

bool canResume(const AgentCli& cli)
{
    return !cli.resumeFlag.empty();
}

std::string makeAgentCommand(const AgentCli& cli,
                             const std::string& command,
                             const std::string& promptArgument,
                             const std::string& sessionArgument)
{
    auto text = command + " " + cli.headlessArguments;

    if (!sessionArgument.empty() && canResume(cli))
        text += " " + cli.resumeFlag + " " + sessionArgument;

    if (!cli.promptFlag.empty())
        text += " " + cli.promptFlag;

    return text + " " + promptArgument;
}
} // namespace diffview
