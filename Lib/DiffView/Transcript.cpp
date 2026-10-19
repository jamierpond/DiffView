#include "Transcript.h"
#include "StreamEvents.h"
#include "Text.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>

namespace diffview
{
namespace
{
using Miro::Json::Value;

constexpr auto promptLead = "> ";
constexpr auto messageLead = "⏺ ";
constexpr auto continuationLead = "  ";
constexpr auto outcomeLead = "  ⎿  ";
constexpr auto thinkingLead = "✻ ";
constexpr auto failureLead = "✗ ";
constexpr auto ellipsis = "…";
constexpr auto millisecondsPerSecond = 1000.0;
constexpr auto secondsPerMinute = 60;
constexpr auto emptyOutcome = "(No content)";
constexpr auto indentUnit = "  ";
constexpr auto subagentConnector = "⎿ ";
constexpr auto hiddenRowsLead = "⋯ ";
constexpr auto forkedCommandPrefix = std::string_view {"forked-command-"};
constexpr auto forkedCommandToolName = "Skill";
constexpr auto backgroundAgentTaskType = "local_agent";
constexpr auto completedTaskStatus = "completed";
constexpr auto maxKeptSubagentEntries = std::size_t {500};

using ToolCallAction = std::function<void(TranscriptEntry&)>;

TranscriptEntry makeEntry(TranscriptEntryKind kind, std::string text = {})
{
    auto entry = TranscriptEntry {};
    entry.kind = kind;
    entry.text = std::move(text);
    return entry;
}

TranscriptRow
    makeRow(TranscriptRowStyle style, std::string lead, std::string text = {})
{
    auto row = TranscriptRow {};
    row.style = style;
    row.lead = std::move(lead);
    row.text = std::move(text);
    return row;
}

std::string joinTextBlocks(const Value& content)
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

        if (type == "image")
            said = "[Image]";

        if (said.empty())
            continue;

        if (!joined.empty())
            joined += "\n";

        joined += said;
    }

    return joined;
}

bool isTrue(const Value& value)
{
    return value.isBool() && value.asBool();
}

ToolOutcome summariseOutcomeText(const std::string& text, bool isError)
{
    auto lines = splitLines(trim(text));
    auto outcome = ToolOutcome {};
    outcome.lineCount = static_cast<int>(lines.size());
    outcome.firstLine =
        lines.empty() ? std::string {emptyOutcome} : displayable(lines.front());
    outcome.isError = isError;
    return outcome;
}

ToolOutcome summariseToolOutcome(const Value& block)
{
    return summariseOutcomeText(joinTextBlocks(getField(block, "content")),
                                isTrue(getField(block, "is_error")));
}

std::string describeToolCall(const Value& block, const std::string& worktree)
{
    auto name = getString(block, "name");
    auto argument = findToolArgument(
        getClaudeCodeTools(), name, getField(block, "input"), worktree);
    auto lines = splitLines(trim(argument));

    if (lines.empty())
        return name;

    auto shown = displayable(lines.front());

    if (lines.size() > 1)
        shown += ellipsis;

    return name + "(" + shown + ")";
}

std::string formatDuration(double milliseconds)
{
    auto seconds = milliseconds / millisecondsPerSecond;
    auto buffer = std::array<char, 32> {};

    if (seconds < secondsPerMinute)
    {
        std::snprintf(buffer.data(), buffer.size(), "%.1fs", seconds);
        return buffer.data();
    }

    auto wholeSeconds = static_cast<int>(seconds);
    std::snprintf(buffer.data(),
                  buffer.size(),
                  "%dm %ds",
                  wholeSeconds / secondsPerMinute,
                  wholeSeconds % secondsPerMinute);
    return buffer.data();
}

std::string formatCost(double dollars)
{
    auto buffer = std::array<char, 32> {};
    std::snprintf(buffer.data(), buffer.size(), "$%.2f", dollars);
    return buffer.data();
}

TranscriptEntry describeResult(const Value& event)
{
    auto entry = makeEntry(TranscriptEntryKind::result);
    auto subtype = getString(event, "subtype");
    const auto& isError = getField(event, "is_error");
    entry.isError = subtype != "success" || (isError.isBool() && isError.asBool());
    entry.text = entry.isError ? "Failed: " + subtype : std::string {"Done"};

    if (const auto& duration = getField(event, "duration_ms"); duration.isNumber())
        entry.text += " in " + formatDuration(duration.asNumber());

    if (const auto& cost = getField(event, "total_cost_usd"); cost.isNumber())
        entry.text += " · " + formatCost(cost.asNumber());

    return entry;
}

bool isSynthetic(const Value& event)
{
    return isTrue(getField(event, "isSynthetic"));
}

void addEntry(std::vector<TranscriptEntry>& entries, TranscriptEntry entry)
{
    auto isRepeatedThinking =
        entry.kind == TranscriptEntryKind::thinking && !entries.empty()
        && entries.back().kind == TranscriptEntryKind::thinking;

    if (!isRepeatedThinking)
        entries.push_back(std::move(entry));
}

void readAssistant(std::vector<TranscriptEntry>& entries,
                   const Value& content,
                   const std::string& worktree)
{
    if (!content.isArray())
        return;

    for (const auto& block: content.asArray())
    {
        auto type = getString(block, "type");

        if (type == "text")
        {
            auto said = trim(getString(block, "text"));

            if (!said.empty())
                addEntry(entries, makeEntry(TranscriptEntryKind::text, said));
        }
        else if (type == "thinking" || type == "redacted_thinking")
        {
            addEntry(entries, makeEntry(TranscriptEntryKind::thinking));
        }
        else if (type == "tool_use")
        {
            auto call = makeEntry(TranscriptEntryKind::toolCall,
                                  describeToolCall(block, worktree));
            call.toolUseId = getString(block, "id");
            addEntry(entries, std::move(call));
        }
    }
}

void attachToolOutcome(std::vector<TranscriptEntry>& entries, const Value& block)
{
    auto id = getString(block, "tool_use_id");

    for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry)
    {
        if (entry->kind == TranscriptEntryKind::toolCall && entry->toolUseId == id)
        {
            entry->outcome = summariseToolOutcome(block);
            return;
        }
    }
}

void attachToolOutcomes(std::vector<TranscriptEntry>& entries, const Value& content)
{
    for (const auto& block: content.asArray())
    {
        if (getString(block, "type") == "tool_result")
            attachToolOutcome(entries, block);
    }
}

bool hasToolResults(const Value& content)
{
    if (!content.isArray())
        return false;

    for (const auto& block: content.asArray())
    {
        if (getString(block, "type") == "tool_result")
            return true;
    }

    return false;
}

// Newest first, nested subagents' calls included.
bool applyToToolCall(std::vector<TranscriptEntry>& entries,
                     const std::string& toolUseId,
                     const ToolCallAction& action)
{
    for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry)
    {
        if (entry->kind == TranscriptEntryKind::toolCall
            && entry->toolUseId == toolUseId)
        {
            action(*entry);
            return true;
        }

        if (applyToToolCall(entry->subagentEntries, toolUseId, action))
            return true;
    }

    return false;
}

void dropOldestSubagentEntries(std::vector<TranscriptEntry>& entries)
{
    if (entries.size() <= maxKeptSubagentEntries)
        return;

    auto excess =
        static_cast<std::ptrdiff_t>(entries.size() - maxKeptSubagentEntries);
    entries.erase(entries.begin(), entries.begin() + excess);
}

void readSubagentMessage(std::vector<TranscriptEntry>& entries,
                         const Value& event,
                         const std::string& worktree)
{
    auto type = getString(event, "type");
    const auto& content = getField(getField(event, "message"), "content");

    if (type == "assistant")
        readAssistant(entries, content, worktree);
    else if (type == "user" && hasToolResults(content))
        attachToolOutcomes(entries, content);
    else if (type == "user")
    {
        if (auto prompt = trim(joinTextBlocks(content)); !prompt.empty())
            addEntry(entries, makeEntry(TranscriptEntryKind::prompt, prompt));
    }

    dropOldestSubagentEntries(entries);
}

// Started from the prompt, a forked skill has no call: its messages arrive
// once it has finished.
TranscriptEntry makeForkedCommandCall(const std::string& parentToolUseId)
{
    auto skillName = parentToolUseId.substr(forkedCommandPrefix.size());
    auto call =
        makeEntry(TranscriptEntryKind::toolCall,
                  std::string {forkedCommandToolName} + "(" + skillName + ")");
    call.toolUseId = parentToolUseId;
    call.outcome = summariseOutcomeText({}, false);
    return call;
}

void readSubagentEvent(std::vector<TranscriptEntry>& entries,
                       const std::string& parentToolUseId,
                       const Value& event,
                       const std::string& worktree)
{
    auto isAttached = applyToToolCall(
        entries,
        parentToolUseId,
        [&](TranscriptEntry& call)
        { readSubagentMessage(call.subagentEntries, event, worktree); });

    if (isAttached || !parentToolUseId.starts_with(forkedCommandPrefix))
        return;

    auto call = makeForkedCommandCall(parentToolUseId);
    readSubagentMessage(call.subagentEntries, event, worktree);
    entries.push_back(std::move(call));
}

void markBackgroundSubagentStarted(std::vector<TranscriptEntry>& entries,
                                   const Value& event)
{
    if (getString(event, "task_type") != backgroundAgentTaskType
        || !isTrue(getField(event, "is_backgrounded")))
        return;

    applyToToolCall(entries,
                    getString(event, "tool_use_id"),
                    [](TranscriptEntry& call) { call.isSubagentRunning = true; });
}

// A background subagent's call has long had its result: the launch notice.
// What it found comes with its notification.
void markBackgroundSubagentFinished(std::vector<TranscriptEntry>& entries,
                                    const Value& event)
{
    applyToToolCall(entries,
                    getString(event, "tool_use_id"),
                    [&](TranscriptEntry& call)
                    {
                        if (!call.isSubagentRunning)
                            return;

                        call.isSubagentRunning = false;
                        auto isFailed =
                            getString(event, "status") != completedTaskStatus;
                        call.outcome = summariseOutcomeText(
                            getString(event, "summary"), isFailed);
                    });
}

void stopEverySubagent(std::vector<TranscriptEntry>& entries)
{
    for (auto& entry: entries)
    {
        entry.isSubagentRunning = false;
        stopEverySubagent(entry.subagentEntries);
    }
}

bool hasRunningSubagentIn(const std::vector<TranscriptEntry>& entries)
{
    return std::any_of(entries.begin(),
                       entries.end(),
                       [](const TranscriptEntry& entry)
                       {
                           return entry.isSubagentRunning
                                  || hasRunningSubagentIn(entry.subagentEntries);
                       });
}

bool isToolCallRunning(const TranscriptEntry& call, bool isParentRunning)
{
    return call.isSubagentRunning || (isParentRunning && !call.outcome);
}

std::optional<std::string> findFinalSubagentText(const TranscriptEntry& call)
{
    if (call.subagentEntries.empty()
        || call.subagentEntries.back().kind != TranscriptEntryKind::text)
        return std::nullopt;

    return call.subagentEntries.back().text;
}

// A finished subagent's answer is its last text; a failure stays a failure.
std::optional<ToolOutcome> findShownOutcome(const TranscriptEntry& call,
                                            bool isRunning)
{
    if (isRunning)
        return std::nullopt;

    if (call.outcome && call.outcome->isError)
        return call.outcome;

    if (auto finalText = findFinalSubagentText(call))
        return summariseOutcomeText(*finalText, false);

    return call.outcome;
}

std::string truncateToWidth(const std::string& text, int width)
{
    if (codepointCount(text) <= width)
        return text;

    return wrap(text, std::max(1, width - 1)).front() + ellipsis;
}

void addWrappedRows(std::vector<TranscriptRow>& rows,
                    TranscriptRowStyle style,
                    const std::string& lead,
                    const std::string& text,
                    int width)
{
    auto textWidth = std::max(1, width - codepointCount(lead));
    auto isFirst = true;

    for (const auto& line: splitLines(text))
    {
        for (auto& piece: wrap(displayable(line), textWidth))
        {
            auto shownLead =
                isFirst ? std::string {lead} : std::string {continuationLead};
            rows.push_back(makeRow(style, shownLead, std::move(piece)));
            isFirst = false;
        }
    }
}

void addEntryRows(std::vector<TranscriptRow>& rows,
                  const TranscriptEntry& entry,
                  int width,
                  bool isParentRunning,
                  const ExpandedSubagents& expandedSubagents);

void keepLastSubagentRows(std::vector<TranscriptRow>& rows)
{
    auto rowCount = static_cast<int>(rows.size());

    if (rowCount <= collapsedSubagentRowCount)
        return;

    auto hiddenCount = rowCount - collapsedSubagentRowCount;
    rows.erase(rows.begin(), rows.begin() + hiddenCount);
    auto noun = hiddenCount == 1 ? " more row" : " more rows";
    rows.insert(rows.begin(),
                makeRow(TranscriptRowStyle::hiddenRows,
                        hiddenRowsLead,
                        std::to_string(hiddenCount) + noun));
}

void addSubagentRows(std::vector<TranscriptRow>& rows,
                     const TranscriptEntry& call,
                     int width,
                     bool isRunning,
                     const ExpandedSubagents& expandedSubagents)
{
    if (call.subagentEntries.empty())
        return;

    auto nestedWidth = std::max(1, width - codepointCount(indentUnit));
    auto nested = std::vector<TranscriptRow> {};

    for (const auto& entry: call.subagentEntries)
        addEntryRows(nested, entry, nestedWidth, isRunning, expandedSubagents);

    if (!expandedSubagents.contains(call.toolUseId))
        keepLastSubagentRows(nested);

    auto isFirst = true;

    for (auto& row: nested)
    {
        auto prefix = isFirst ? subagentConnector : indentUnit;
        row.indent = prefix + row.indent;
        rows.push_back(std::move(row));
        isFirst = false;
    }
}

void addToolCallRows(std::vector<TranscriptRow>& rows,
                     const TranscriptEntry& entry,
                     int width,
                     bool isParentRunning,
                     const ExpandedSubagents& expandedSubagents)
{
    auto isRunning = isToolCallRunning(entry, isParentRunning);
    auto call = makeRow(TranscriptRowStyle::toolCall, messageLead);
    call.text = truncateToWidth(entry.text, width - codepointCount(messageLead));
    call.isInFlight = isRunning;

    if (!entry.subagentEntries.empty())
        call.subagentToolUseId = entry.toolUseId;

    rows.push_back(std::move(call));
    addSubagentRows(rows, entry, width, isRunning, expandedSubagents);

    auto shownOutcome = findShownOutcome(entry, isRunning);

    if (!shownOutcome)
        return;

    const auto& outcome = *shownOutcome;
    auto style = outcome.isError ? TranscriptRowStyle::toolError
                                 : TranscriptRowStyle::toolOutcome;
    auto row = makeRow(style, outcomeLead);
    auto moreLines = outcome.lineCount - 1;

    if (moreLines > 0)
        row.detail = " (+" + std::to_string(moreLines)
                     + (moreLines == 1 ? " line)" : " lines)");

    auto textWidth =
        width - codepointCount(outcomeLead) - codepointCount(row.detail);

    if (textWidth < 1)
    {
        row.detail.clear();
        textWidth = width - codepointCount(outcomeLead);
    }

    row.text = truncateToWidth(outcome.firstLine, textWidth);
    rows.push_back(std::move(row));
}

void addEntryRows(std::vector<TranscriptRow>& rows,
                  const TranscriptEntry& entry,
                  int width,
                  bool isParentRunning,
                  const ExpandedSubagents& expandedSubagents)
{
    switch (entry.kind)
    {
        case TranscriptEntryKind::prompt:
            addWrappedRows(
                rows, TranscriptRowStyle::prompt, promptLead, entry.text, width);
            return;
        case TranscriptEntryKind::text:
            addWrappedRows(
                rows, TranscriptRowStyle::text, messageLead, entry.text, width);
            return;
        case TranscriptEntryKind::thinking:
            rows.push_back(
                makeRow(TranscriptRowStyle::thinking, thinkingLead, "Thinking…"));
            return;
        case TranscriptEntryKind::toolCall:
            addToolCallRows(rows, entry, width, isParentRunning, expandedSubagents);
            return;
        case TranscriptEntryKind::result:
            if (entry.isError)
                addWrappedRows(rows,
                               TranscriptRowStyle::failedResult,
                               failureLead,
                               entry.text,
                               width);
            else
                addWrappedRows(rows,
                               TranscriptRowStyle::result,
                               thinkingLead,
                               entry.text,
                               width);
            return;
    }
}
} // namespace

Transcript::Transcript(std::string worktreeToUse)
    : worktree(std::move(worktreeToUse))
{
}

void Transcript::append(std::string_view bytes)
{
    partialLine += bytes;
    auto start = std::size_t {0};

    for (auto end = partialLine.find('\n'); end != std::string::npos;
         end = partialLine.find('\n', start))
    {
        readLine(std::string_view {partialLine}.substr(start, end - start));
        start = end + 1;
    }

    partialLine.erase(0, start);
}

void Transcript::addQueuedPrompt(const std::string& text)
{
    queuedPrompts.push_back(trim(text));
}

const std::vector<std::string>& Transcript::getQueuedPrompts() const
{
    return queuedPrompts;
}

const std::vector<TranscriptEntry>& Transcript::getEntries() const
{
    return entries;
}

bool Transcript::isTurnOpen() const
{
    return turnOpen;
}

bool Transcript::hasRunningSubagent() const
{
    return hasRunningSubagentIn(entries);
}

void Transcript::readLine(std::string_view line)
{
    auto event = Miro::Json::getParsedValue(line);

    if (isSynthetic(event))
        return;

    if (auto parent = getString(event, "parent_tool_use_id"); !parent.empty())
    {
        readSubagentEvent(entries, parent, event, worktree);
        return;
    }

    auto type = getString(event, "type");
    const auto& content = getField(getField(event, "message"), "content");

    if (type == "assistant")
    {
        readAssistant(entries, content, worktree);
        return;
    }

    if (type == "user" && hasToolResults(content))
    {
        attachToolOutcomes(entries, content);
        return;
    }

    if (type == "user")
    {
        auto prompt = trim(joinTextBlocks(content));

        if (prompt.empty())
            return;

        auto queued = std::find(queuedPrompts.begin(), queuedPrompts.end(), prompt);

        if (queued != queuedPrompts.end())
            queuedPrompts.erase(queued);

        addEntry(entries, makeEntry(TranscriptEntryKind::prompt, prompt));
        turnOpen = true;
        return;
    }

    if (type == "result")
    {
        addEntry(entries, describeResult(event));
        turnOpen = false;
        return;
    }

    if (type != "system")
        return;

    auto subtype = getString(event, "subtype");

    if (subtype == "task_started")
        markBackgroundSubagentStarted(entries, event);
    else if (subtype == "task_notification")
        markBackgroundSubagentFinished(entries, event);
    else if (subtype == "init")
    {
        // A fresh start after a crash: what it was doing never finishes.
        turnOpen = false;
        stopEverySubagent(entries);
    }
}

std::vector<TranscriptRow>
    layoutTranscript(const Transcript& transcript,
                     int width,
                     const ExpandedSubagents& expandedSubagents,
                     int queuedMessageCount)
{
    auto rows = std::vector<TranscriptRow> {};

    for (const auto& entry: transcript.getEntries())
    {
        if (!rows.empty())
            rows.push_back({});

        addEntryRows(rows, entry, width, transcript.isTurnOpen(), expandedSubagents);
    }

    if (transcript.isTurnOpen())
    {
        if (!rows.empty())
            rows.push_back({});

        auto working =
            makeRow(TranscriptRowStyle::working, thinkingLead, "Working…");
        working.isInFlight = true;
        rows.push_back(std::move(working));

        if (queuedMessageCount > 0)
            rows.push_back(makeRow(TranscriptRowStyle::queuedCount,
                                   hiddenRowsLead,
                                   std::to_string(queuedMessageCount) + " queued"));
    }

    for (const auto& queued: transcript.getQueuedPrompts())
    {
        if (!rows.empty())
            rows.push_back({});

        addWrappedRows(
            rows, TranscriptRowStyle::queuedPrompt, promptLead, queued, width);
    }

    return rows;
}
} // namespace diffview
