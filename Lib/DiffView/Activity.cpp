#include "Activity.h"
#include "Text.h"
#include "Wake.h"

#include <charconv>
#include <utility>

namespace diffview
{
namespace
{
constexpr auto threadKey = std::string_view {"thread"};
constexpr auto updatedKey = std::string_view {"updated"};
constexpr auto doingKey = std::string_view {"doing"};

int parseNumber(std::string_view text)
{
    auto value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

std::vector<std::string> splitWords(std::string_view text)
{
    auto words = std::vector<std::string> {};
    auto word = std::string {};

    for (auto c: text)
    {
        if (c != ' ')
            word += c;
        else if (!word.empty())
            words.push_back(std::exchange(word, {}));
    }

    if (!word.empty())
        words.push_back(word);

    return words;
}

// "dv-wake pid 123 agent dev cli claude threads 4 7": pairs, then the threads
// to the end.
void readWakeHeader(WakeProgress& progress, std::string_view line)
{
    auto words = splitWords(line);

    for (std::size_t i = 1; i + 1 < words.size(); i += 2)
    {
        if (words[i] == "threads")
        {
            for (auto at = i + 1; at < words.size(); ++at)
                progress.threads.push_back(parseNumber(words[at]));

            return;
        }

        if (words[i] == "pid")
            progress.pid = parseNumber(words[i + 1]);
        else if (words[i] == "agent")
            progress.agent = words[i + 1];
        else if (words[i] == "cli")
            progress.cli = words[i + 1];
    }
}

void readWakeLine(WakeProgress& progress,
                  std::string_view line,
                  const std::string& worktree)
{
    auto exitPrefix = std::string {wakeExitWord} + " ";
    auto headerPrefix = std::string {wakeHeaderWord} + " ";

    if (line.starts_with(exitPrefix))
        progress.exitCode = parseNumber(line.substr(exitPrefix.size()));
    else if (line.starts_with(headerPrefix))
        readWakeHeader(progress, line);
    else if (auto doing =
                 getAgentCli(progress.cli).summariseStreamLine(line, worktree))
        progress.doing = *doing;
}
} // namespace

void readWakeLog(WakeProgress& progress,
                 std::string_view text,
                 const std::string& worktree)
{
    progress.partialLine += text;
    auto start = std::size_t {0};

    for (auto end = progress.partialLine.find('\n'); end != std::string::npos;
         end = progress.partialLine.find('\n', start))
    {
        readWakeLine(
            progress,
            std::string_view {progress.partialLine}.substr(start, end - start),
            worktree);
        start = end + 1;
    }

    progress.partialLine.erase(0, start);
}

std::string formatAgentStatus(const AgentStatus& status)
{
    auto lines = splitLines(trim(status.doing));
    auto doing = lines.empty() ? std::string {} : lines.front();

    return std::string {threadKey} + " " + std::to_string(status.thread) + "\n"
           + std::string {updatedKey} + " " + std::to_string(status.updated) + "\n"
           + std::string {doingKey} + " " + doing + "\n";
}

AgentStatus parseAgentStatus(const std::string& text)
{
    auto status = AgentStatus {};

    for (const auto& line: splitLines(text))
    {
        auto space = line.find(' ');

        if (space == std::string::npos)
            continue;

        auto key = std::string_view {line}.substr(0, space);
        auto value = line.substr(space + 1);

        if (key == threadKey)
            status.thread = parseNumber(value);
        else if (key == updatedKey)
            status.updated = static_cast<std::time_t>(std::stoll("0" + value));
        else if (key == doingKey)
            status.doing = value;
    }

    return status;
}

bool isAgentStatusCurrent(const AgentStatus& status, std::time_t now)
{
    return status.thread > 0 && !status.doing.empty()
           && now - status.updated < statusExpirySeconds;
}

std::string describeActivity(const AgentActivity& activity)
{
    auto name = "@" + activity.agent;

    if (activity.hasFailed)
        return name + " failed · " + activity.doing;

    return name + " is working · " + activity.doing;
}

bool isActivityFile(std::string_view path)
{
    auto worktrees = std::string {worktreeDirectoryName} + "/";

    if (path.starts_with(worktrees))
    {
        auto slash = path.find('/', worktrees.size());

        if (slash == std::string_view::npos)
            return false;

        path.remove_prefix(slash + 1);
    }

    return path.starts_with(std::string {wakeDirectory} + "/")
           || path.starts_with(std::string {statusDirectory} + "/");
}
} // namespace diffview
