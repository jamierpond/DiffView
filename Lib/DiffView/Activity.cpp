#include "Activity.h"
#include "Text.h"
#include "Wake.h"

#include <Miro/Json.h>

#include <charconv>
#include <utility>
#include <map>

namespace diffview
{
namespace
{
using Miro::Json::Value;

constexpr auto maxSummaryLength = 60;
constexpr auto threadKey = std::string_view {"thread"};
constexpr auto updatedKey = std::string_view {"updated"};
constexpr auto doingKey = std::string_view {"doing"};

struct ToolSummary
{
    std::string_view verb;
    std::string_view argument;
};

const Value& getField(const Value& value, std::string_view key)
{
    static const auto missing = Value {};

    if (!value.isObject())
        return missing;

    if (const auto* found = Miro::Json::find(value.asObject(), key))
        return *found;

    return missing;
}

std::string getString(const Value& value, std::string_view key)
{
    const auto& field = getField(value, key);

    if (!field.isString())
        return {};

    return field.asString();
}

std::string shorten(std::string_view text)
{
    auto lines = splitLines(trim(text));

    if (lines.empty())
        return {};

    const auto& first = lines.front();

    if (codepointCount(first) <= maxSummaryLength && lines.size() == 1)
        return first;

    return wrap(first, maxSummaryLength - 1).front() + "…";
}

std::string makeRelative(const std::string& path, const std::string& worktree)
{
    auto prefix = worktree + "/";

    if (!worktree.empty() && path.starts_with(prefix))
        return path.substr(prefix.size());

    return path;
}

std::string summariseToolUse(const Value& block, const std::string& worktree)
{
    static const auto tools = std::map<std::string, ToolSummary, std::less<>> {
        {"Bash", {"running", "command"}},
        {"Read", {"reading", "file_path"}},
        {"Edit", {"editing", "file_path"}},
        {"MultiEdit", {"editing", "file_path"}},
        {"Write", {"writing", "file_path"}},
        {"NotebookEdit", {"editing", "notebook_path"}},
        {"Grep", {"searching for", "pattern"}},
        {"Glob", {"finding", "pattern"}},
        {"Agent", {"delegating:", "description"}},
        {"Task", {"delegating:", "description"}},
        {"WebFetch", {"fetching", "url"}},
        {"WebSearch", {"searching the web for", "query"}},
    };

    auto name = getString(block, "name");
    auto found = tools.find(name);

    if (found == tools.end())
        return shorten("using " + name);

    const auto& [verb, key] = found->second;
    auto argument = makeRelative(getString(getField(block, "input"), key), worktree);

    return shorten(std::string {verb} + " " + argument);
}

std::optional<std::string> summariseAssistant(const Value& event,
                                              const std::string& worktree)
{
    const auto& content = getField(getField(event, "message"), "content");

    if (!content.isArray() || content.asArray().empty())
        return std::nullopt;

    const auto& block = content.asArray().back();
    auto type = getString(block, "type");

    if (type == "tool_use")
        return summariseToolUse(block, worktree);

    if (type == "thinking" || type == "redacted_thinking")
        return "thinking";

    if (type == "text")
    {
        if (auto said = shorten(getString(block, "text")); !said.empty())
            return said;
    }

    return std::nullopt;
}

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

// "dv-wake pid 123 agent dev threads 4 7": pairs, then the threads to the end.
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
    else if (auto doing = summariseStreamEvent(line, worktree))
        progress.doing = *doing;
}
} // namespace

std::optional<std::string> summariseStreamEvent(std::string_view line,
                                                const std::string& worktree)
{
    auto event = Miro::Json::getParsedValue(line);
    auto type = getString(event, "type");

    if (type == "assistant")
        return summariseAssistant(event, worktree);

    if (type == "user")
        return "thinking";

    if (type == "system" && getString(event, "subtype") == "init")
        return "starting";

    return std::nullopt;
}

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
