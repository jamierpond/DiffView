#pragma once

#include <optional>
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
};

// What Claude Code's `--output-format stream-json --verbose` says, read as it
// is written: the user's prompts (with --replay-user-messages), its text,
// thinking, tool calls with their results, and each turn's result.
// Subagents' messages are left out, as Claude Code's own UI does.
class Transcript
{
public:
    explicit Transcript(std::string worktree);

    // Any bytes: a line split across calls is read once it ends.
    void append(std::string_view bytes);

    const std::vector<TranscriptEntry>& getEntries() const;
    bool isTurnOpen() const;

private:
    void readLine(std::string_view line);

    std::string worktree;
    std::string partialLine;
    std::vector<TranscriptEntry> entries;
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
    result,
    failedResult,
    working
};

struct TranscriptRow
{
    TranscriptRowStyle style = TranscriptRowStyle::blank;
    std::string lead;
    std::string text;
    std::string detail;
    // Its lead is replaced by a spinner.
    bool isInFlight = false;
};

// Claude Code's look, every row at most `width` columns.
std::vector<TranscriptRow> layoutTranscript(const Transcript& transcript, int width);
} // namespace diffview
