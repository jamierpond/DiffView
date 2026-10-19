#include <Text.h>
#include <Transcript.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto worktree = "/r/main";
constexpr auto narrowWidth = 30;
constexpr auto wideWidth = 80;

const auto prompt =
    std::string {
        R"({"type":"user","message":{"role":"user","content":"#46: reply on #46"}})"}
    + "\n";
const auto thinking =
    std::string {
        R"({"type":"assistant","message":{"content":[{"type":"thinking","thinking":""}]}})"}
    + "\n";
const auto toolCall =
    std::string {
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t1","name":"Bash","input":{"command":"dv show 46\nmore"}}]}})"}
    + "\n";
const auto readCall =
    std::string {
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t2","name":"Read","input":{"file_path":"/r/main/Source/Foo.cpp"}}]}})"}
    + "\n";
const auto toolResult =
    std::string {
        R"({"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"t1","content":"first line\nsecond\nthird"}]}})"}
    + "\n";
const auto failedRead =
    std::string {
        R"({"type":"user","message":{"content":[{"tool_use_id":"t2","type":"tool_result","is_error":true,"content":[{"type":"text","text":"No such file"}]}]}})"}
    + "\n";
const auto answer =
    std::string {
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"Replied on #46."}]}})"}
    + "\n";
const auto result =
    std::string {
        R"({"type":"result","subtype":"success","is_error":false,"duration_ms":7680,"total_cost_usd":4.032})"}
    + "\n";

std::vector<TranscriptEntryKind> getKinds(const Transcript& transcript)
{
    auto kinds = std::vector<TranscriptEntryKind> {};

    for (const auto& entry: transcript.getEntries())
        kinds.push_back(entry.kind);

    return kinds;
}

Transcript makeTranscript(const std::string& stream)
{
    auto transcript = Transcript {worktree};
    transcript.append(stream);
    return transcript;
}
} // namespace

auto tTranscriptTurn =
    test("Transcript/a turn reads as prompt, work and result") = []
{
    auto transcript =
        makeTranscript(prompt + thinking + thinking + toolCall + readCall
                       + toolResult + failedRead + answer + result);
    const auto& entries = transcript.getEntries();
    using Kind = TranscriptEntryKind;

    check(getKinds(transcript)
          == std::vector {Kind::prompt,
                          Kind::thinking,
                          Kind::toolCall,
                          Kind::toolCall,
                          Kind::text,
                          Kind::result});
    check(entries[0].text == "#46: reply on #46");
    check(entries[2].text == "Bash(dv show 46…)");
    check(entries[2].outcome && entries[2].outcome->firstLine == "first line");
    check(entries[2].outcome->lineCount == 3 && !entries[2].outcome->isError);
    check(entries[3].text == "Read(Source/Foo.cpp)");
    check(entries[3].outcome && entries[3].outcome->isError);
    check(entries[5].text == "Done in 7.7s · $4.03" && !entries[5].isError);
    check(!transcript.isTurnOpen());
};

auto tTranscriptPartialLine =
    test("Transcript/a line split across reads is read once it ends") = []
{
    auto transcript = Transcript {worktree};
    auto half = prompt.size() / 2;

    transcript.append(prompt.substr(0, half));
    check(transcript.getEntries().empty());

    transcript.append(prompt.substr(half));
    check(transcript.getEntries().size() == 1);
    check(transcript.isTurnOpen());
};

auto tTranscriptSkips =
    test("Transcript/subagents, synthetic messages and noise are left out") = []
{
    auto subagent = std::string {
        R"({"type":"assistant","parent_tool_use_id":"t9","message":{"content":[{"type":"text","text":"inner"}]}})"
        "\n"};
    auto synthetic = std::string {
        R"({"type":"user","isSynthetic":true,"message":{"content":[{"type":"text","text":"[Image]"}]}})"
        "\n"};
    auto transcript = makeTranscript(subagent + synthetic + "not json\n"
                                     + R"({"type":"rate_limit_event"})" + "\n");

    check(transcript.getEntries().empty());
};

auto tTranscriptRestart =
    test("Transcript/a fresh start closes a turn that never finished") = []
{
    auto transcript = makeTranscript(prompt + toolCall);
    check(transcript.isTurnOpen());

    transcript.append(R"({"type":"system","subtype":"init"})"
                      "\n");
    check(!transcript.isTurnOpen());
};

auto tTranscriptLayout = test("Transcript/rows look like Claude Code's") = []
{
    auto transcript =
        makeTranscript(prompt + thinking + toolCall + toolResult + answer + result);
    auto rows = layoutTranscript(transcript, wideWidth);
    using Style = TranscriptRowStyle;

    check(rows.size() == 10);
    check(rows[0].style == Style::prompt && rows[0].lead == "> ");
    check(rows[1].style == Style::blank);
    check(rows[2].style == Style::thinking && rows[2].text == "Thinking…");
    check(rows[4].style == Style::toolCall && rows[4].lead == "⏺ ");
    check(rows[4].text == "Bash(dv show 46…)" && !rows[4].isInFlight);
    check(rows[5].style == Style::toolOutcome && rows[5].lead == "  ⎿  ");
    check(rows[5].text == "first line" && rows[5].detail == " (+2 lines)");
    check(rows[7].style == Style::text && rows[7].text == "Replied on #46.");
    check(rows[9].style == Style::result && rows[9].text == "Done in 7.7s · $4.03");
};

auto tTranscriptLayoutInFlight =
    test("Transcript/an open turn spins on its pending call and ends working") = []
{
    auto rows = layoutTranscript(makeTranscript(prompt + toolCall), wideWidth);
    using Style = TranscriptRowStyle;

    check(rows.back().style == Style::working && rows.back().isInFlight);
    check(rows[2].style == Style::toolCall && rows[2].isInFlight);
};

auto tTranscriptLayoutWidth =
    test("Transcript/rows fit the width: text wraps, calls are cut") = []
{
    auto longText = std::string(narrowWidth * 2, 'x');
    auto stream =
        prompt
        + R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t1","name":"Bash","input":{"command":")"
        + longText + R"("}},{"type":"text","text":")" + longText + R"("}]}})" + "\n";
    auto rows = layoutTranscript(makeTranscript(stream), narrowWidth);

    for (const auto& row: rows)
        check(codepointCount(row.lead + row.text + row.detail) <= narrowWidth);

    auto textRows = 0;

    for (const auto& row: rows)
    {
        if (row.style == TranscriptRowStyle::text)
            ++textRows;
    }

    check(textRows == 3);
    check(rows[2].text.ends_with("…"));
};

auto tTranscriptReplayedPrompt = test(
    "Transcript/a replayed typed message is a prompt, its text blocks joined") = []
{
    auto replayed = std::string {
        R"({"type":"user","message":{"role":"user","content":[{"text":"hello how are you ?","type":"text"}]},"session_id":"79275283-a313-4d81-9330-913343680bfc","parent_tool_use_id":null,"uuid":"d0d4bb37-4f41-4586-9953-ae384d231b5f","timestamp":"2026-10-19T07:31:53.416Z","isReplay":true})"
        "\n"};
    auto twoBlocks = std::string {
        R"({"type":"user","message":{"role":"user","content":[{"text":"one","type":"text"},{"text":"two","type":"text"}]}})"
        "\n"};
    auto transcript = makeTranscript(replayed + twoBlocks + toolCall + toolResult);
    const auto& entries = transcript.getEntries();

    check(entries.size() == 3);
    check(entries[0].kind == TranscriptEntryKind::prompt);
    check(entries[0].text == "hello how are you ?");
    check(entries[1].kind == TranscriptEntryKind::prompt
          && entries[1].text == "one\ntwo");
    check(entries[2].kind == TranscriptEntryKind::toolCall && entries[2].outcome);
};

auto tTranscriptQueuedPrompt =
    test("Transcript/a sent prompt shows queued until the CLI replays it") = []
{
    auto transcript = makeTranscript(prompt + toolCall);
    transcript.addQueuedPrompt("hello how are you ?");

    auto rows = layoutTranscript(transcript, wideWidth);
    check(rows.back().style == TranscriptRowStyle::queuedPrompt);
    check(rows.back().text == "hello how are you ?");

    transcript.append(
        result
        + R"({"type":"user","message":{"role":"user","content":[{"text":"hello how are you ?","type":"text"}]},"isReplay":true})"
        + "\n");
    check(transcript.getQueuedPrompts().empty());
    check(transcript.getEntries().back().kind == TranscriptEntryKind::prompt);
    check(transcript.getEntries().back().text == "hello how are you ?");
};
