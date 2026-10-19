#include "StreamEvents.h"
#include "Text.h"

namespace diffview
{
namespace
{
using Miro::Json::Value;

constexpr auto maxSummaryLength = 60;

std::string makeRelative(const std::string& path, const std::string& worktree)
{
    auto prefix = worktree + "/";

    if (!worktree.empty() && path.starts_with(prefix))
        return path.substr(prefix.size());

    return path;
}
} // namespace

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

std::string summariseToolCall(const ToolSummaries& tools,
                              const std::string& name,
                              const Value& input,
                              const std::string& worktree)
{
    auto found = tools.find(name);

    if (found == tools.end())
        return shortenSummary("using " + name);

    const auto& [verb, key] = found->second;
    auto argument = makeRelative(getString(input, key), worktree);

    return shortenSummary(std::string {verb} + " " + argument);
}
} // namespace diffview
