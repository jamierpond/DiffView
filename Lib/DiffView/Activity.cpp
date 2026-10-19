#include "Activity.h"
#include "Responder.h"
#include "Text.h"

#include <charconv>

namespace diffview
{
namespace
{
constexpr auto commentThreadKey = std::string_view {"thread"};
constexpr auto updatedKey = std::string_view {"updated"};
constexpr auto doingKey = std::string_view {"doing"};

int parseNumber(std::string_view text)
{
    auto value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}
} // namespace

std::string formatAgentStatus(const AgentStatus& status)
{
    auto lines = splitLines(trim(status.doing));
    auto doing = lines.empty() ? std::string {} : lines.front();

    return std::string {commentThreadKey} + " "
           + std::to_string(status.commentThread) + "\n" + std::string {updatedKey}
           + " " + std::to_string(status.updated) + "\n" + std::string {doingKey}
           + " " + doing + "\n";
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

        if (key == commentThreadKey)
            status.commentThread = parseNumber(value);
        else if (key == updatedKey)
            status.updated = static_cast<std::time_t>(std::stoll("0" + value));
        else if (key == doingKey)
            status.doing = value;
    }

    return status;
}

bool isAgentStatusCurrent(const AgentStatus& status, std::time_t now)
{
    return status.commentThread > 0 && !status.doing.empty()
           && now - status.updated < statusExpirySeconds;
}

std::string describeActivity(const AgentActivity& activity)
{
    return "@" + activity.agent + " is working · " + activity.doing;
}

bool isActivityFile(std::string_view path)
{
    return path.starts_with(std::string {responderDirectory} + "/")
           || path.starts_with(std::string {statusDirectory} + "/");
}
} // namespace diffview
