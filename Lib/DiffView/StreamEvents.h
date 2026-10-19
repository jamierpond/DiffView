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

// A message's text blocks, a line each; "[Image]" for an image.
std::string joinTextBlocks(const Miro::Json::Value& content);

// A message's text blocks, a line each; images left out.
std::string joinOnlyTextBlocks(const Miro::Json::Value& content);

// One short line: the first line of the text, cut with "…" if it runs on.
std::string shortenSummary(std::string_view text);

struct ToolSummary
{
    std::string_view verb;
    std::string_view argumentKey;
};

using ToolSummaries = std::map<std::string, ToolSummary, std::less<>>;

// The tool's key argument, made relative to the worktree; empty for a tool not
// in the table.
std::string findToolArgument(const ToolSummaries& tools,
                             const std::string& name,
                             const Miro::Json::Value& input,
                             const std::string& worktree);

// "editing Source/Foo.cpp": the tool's verb and its key argument, made
// relative to the worktree; "using <name>" for a tool not in the table.
std::string summariseToolCall(const ToolSummaries& tools,
                              const std::string& name,
                              const Miro::Json::Value& input,
                              const std::string& worktree);

const ToolSummaries& getClaudeCodeTools();
} // namespace diffview
