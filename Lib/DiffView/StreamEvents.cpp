#include "StreamEvents.h"
#include "Text.h"

namespace diffview
{
namespace
{
using Miro::Json::Value;

constexpr auto maxSummaryLength = 60;
constexpr auto peerOriginKind = "peer";
constexpr auto peerMessageLead =
    std::string_view {"Another Claude session sent a message"};
constexpr auto peerMessageTrailer =
    std::string_view {"\n\nThis came from another Claude session"};

std::string makeRelative(const std::string& path, const std::string& worktree)
{
    auto prefix = worktree + "/";

    if (!worktree.empty() && path.starts_with(prefix))
        return path.substr(prefix.size());

    return path;
}

std::string joinBlocks(const Miro::Json::Value& content, bool shouldMarkImages)
{
    if (content.isString())
        return content.asString();

    if (!content.isArray())
        return {};

    auto joined = std::string {};

    for (const auto& block: content.asArray())
    {
        auto type = getString(block, "type");
        auto said = type == "text" ? getString(block, "text") : std::string {};

        if (type == "image" && shouldMarkImages)
            said = "[Image]";

        if (said.empty())
            continue;

        if (!joined.empty())
            joined += "\n";

        joined += said;
    }

    return joined;
}
} // namespace

std::string joinTextBlocks(const Miro::Json::Value& content)
{
    return joinBlocks(content, true);
}

std::string joinOnlyTextBlocks(const Miro::Json::Value& content)
{
    return joinBlocks(content, false);
}

bool isPeerMessage(const Value& event)
{
    return getString(event, "type") == "user"
           && getString(getField(event, "origin"), "kind") == peerOriginKind;
}

std::string extractPeerMessageText(const Value& event)
{
    auto text = joinTextBlocks(getField(getField(event, "message"), "content"));

    if (!text.starts_with(peerMessageLead))
        return text;

    auto bodyStart = text.find('\n');

    if (bodyStart == std::string::npos)
        return text;

    auto body = text.substr(bodyStart + 1);
    auto trailer = body.rfind(peerMessageTrailer);

    if (trailer != std::string::npos)
        body.erase(trailer);

    return body;
}

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

std::string shortenSummary(std::string_view text)
{
    auto lines = splitLines(trim(text));

    if (lines.empty())
        return {};

    const auto& first = lines.front();

    if (codepointCount(first) <= maxSummaryLength && lines.size() == 1)
        return first;

    return wrap(first, maxSummaryLength - 1).front() + "…";
}

std::string findToolArgument(const ToolSummaries& tools,
                             const std::string& name,
                             const Value& input,
                             const std::string& worktree)
{
    auto found = tools.find(name);

    if (found == tools.end())
        return {};

    return makeRelative(getString(input, found->second.argumentKey), worktree);
}

std::string summariseToolCall(const ToolSummaries& tools,
                              const std::string& name,
                              const Value& input,
                              const std::string& worktree)
{
    auto found = tools.find(name);

    if (found == tools.end())
        return shortenSummary("using " + name);

    auto argument = findToolArgument(tools, name, input, worktree);
    return shortenSummary(std::string {found->second.verb} + " " + argument);
}
} // namespace diffview
