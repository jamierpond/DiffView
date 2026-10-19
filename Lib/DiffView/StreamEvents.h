#pragma once

#include <Miro/Json.h>

#include <map>
#include <string>
#include <string_view>

namespace diffview
{
const Miro::Json::Value& getField(const Miro::Json::Value& value,
                                  std::string_view key);
std::string getString(const Miro::Json::Value& value, std::string_view key);

// One short line: the first line of the text, cut with "…" if it runs on.
std::string shortenSummary(std::string_view text);

struct ToolSummary
{
    std::string_view verb;
    std::string_view argumentKey;
};

using ToolSummaries = std::map<std::string, ToolSummary, std::less<>>;

// "editing Source/Foo.cpp": the tool's verb and its key argument, made
// relative to the worktree; "using <name>" for a tool not in the table.
std::string summariseToolCall(const ToolSummaries& tools,
                              const std::string& name,
                              const Miro::Json::Value& input,
                              const std::string& worktree);
} // namespace diffview
