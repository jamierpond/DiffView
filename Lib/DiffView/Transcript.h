#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
enum class TranscriptEntryKind
{
    prompt,
    text,
    thinking,
    toolCall,
    result
};

struct ToolOutcome
{
    std::string firstLine;
    int lineCount = 0;
    bool isError = false;
};

struct TranscriptEntry
{
    TranscriptEntryKind kind = TranscriptEntryKind::text;
    // A tool call's is "Name(argument)"; a result's its summary.
    std::string text;
    std::string toolUseId;
    std::optional<ToolOutcome> outcome;
    bool isError = false;
    // A call that started a subagent: what the subagent did, its tail.
    std::vector<TranscriptEntry> subagentEntries;
    // A background subagent outlives its call's result.
    bool isSubagentRunning = false;
};

// What Claude Code's `--output-format stream-json --verbose` says, read as it
// is written: the user's prompts (with --replay-user-messages), its text,
// thinking, tool calls with their results, and each turn's result.
// A subagent's messages (with --forward-subagent-text, its text and thinking
// too) nest under the call that started it.
class Transcript
{
public:
    explicit Transcript(std::string worktree);

    // Any bytes: a line split across calls is read once it ends.
    void append(std::string_view bytes);

    // Sent but not yet read back: the CLI replays a message only once it
    // takes it, after any turn in flight.
    void addQueuedPrompt(const std::string& text);

    const std::vector<TranscriptEntry>& getEntries() const;
    const std::vector<std::string>& getQueuedPrompts() const;
    bool isTurnOpen() const;
    bool hasRunningSubagent() const;

private:
    void readLine(std::string_view line);

    std::string worktree;
    std::string partialLine;
    std::vector<TranscriptEntry> entries;
    std::vector<std::string> queuedPrompts;
    bool turnOpen = false;
};

enum class TranscriptRowStyle
{
    blank,
    prompt,
    text,
    thinking,
    toolCall,
    toolOutcome,
    toolError,
    hiddenRows,
    result,
    failedResult,
    working,
    queuedPrompt
};

struct TranscriptRow
{
    TranscriptRowStyle style = TranscriptRowStyle::blank;
    // Before the lead: two columns per level of subagent nesting.
    std::string indent;
    std::string lead;
    std::string text;
    std::string detail;
    // Its lead is replaced by a spinner.
    bool isInFlight = false;
    // Set on a call's row when it has a subagent block that za folds.
    std::string subagentToolUseId;
};

// Unexpanded, a subagent's block shows only this many of its last rows.
constexpr auto collapsedSubagentRowCount = 3;

using ExpandedSubagents = std::set<std::string>;

// Claude Code's look, every row at most `width` columns, indent included.
std::vector<TranscriptRow>
    layoutTranscript(const Transcript& transcript,
                     int width,
                     const ExpandedSubagents& expandedSubagents = {});
} // namespace diffview
