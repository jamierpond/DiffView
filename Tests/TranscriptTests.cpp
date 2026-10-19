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

std::string makeAgentCall(const std::string& id,
                          const std::string& description,
                          const std::string& parent)
{
    return R"({"type":"assistant","message":{"model":"claude-opus-5-5","type":"message","role":"assistant","content":[{"type":"tool_use","id":")"
           + id + R"(","name":"Agent","input":{"description":")" + description
           + R"(","subagent_type":"general-purpose","prompt":"Go."},"caller":{"type":"direct"}}]},"parent_tool_use_id":)"
           + parent + R"(,"session_id":"79275283"})" + "\n";
}

std::string quoted(const std::string& text)
{
    return "\"" + text + "\"";
}

std::string makeSubagentPrompt(const std::string& parent)
{
    return R"({"type":"user","message":{"role":"user","content":[{"type":"text","text":"Read CLAUDE.md first."}]},"parent_tool_use_id":")"
           + parent + R"(","session_id":"79275283"})" + "\n";
}

std::string makeSubagentThinking(const std::string& parent)
{
    return R"({"type":"assistant","message":{"model":"claude-opus-5-5","type":"message","role":"assistant","content":[{"type":"thinking","thinking":"","signature":"CAQS"}]},"parent_tool_use_id":")"
           + parent + R"(","session_id":"79275283"})" + "\n";
}

std::string makeSubagentBash(const std::string& parent, const std::string& id)
{
    return R"({"type":"assistant","message":{"model":"claude-opus-5-5","type":"message","role":"assistant","content":[{"type":"tool_use","id":")"
           + id
           + R"(","name":"Bash","input":{"command":"cat Lib/DiffView/Transcript.h"},"caller":{"type":"direct"}}]},"parent_tool_use_id":")"
           + parent + R"(","session_id":"79275283"})" + "\n";
}

std::string makeSubagentBashResult(const std::string& parent, const std::string& id)
{
    return R"({"type":"user","message":{"role":"user","content":[{"tool_use_id":")"
           + id
           + R"(","type":"tool_result","content":"#pragma once\n\n#include <optional>","is_error":false}]},"parent_tool_use_id":")"
           + parent + R"(","agent_id":"acee39ba847179220","session_id":"79275283"})"
           + "\n";
}

std::string makeSubagentText(const std::string& parent, const std::string& said)
{
    return R"({"type":"assistant","message":{"model":"claude-opus-5-5","type":"message","role":"assistant","content":[{"type":"text","text":")"
           + said + R"("}]},"parent_tool_use_id":")" + parent
           + R"(","session_id":"79275283"})" + "\n";
}

std::string makeToolResult(const std::string& id,
                           const std::string& said,
                           const std::string& parent)
{
    return R"({"type":"user","message":{"role":"user","content":[{"tool_use_id":")"
           + id + R"(","type":"tool_result","content":[{"type":"text","text":")"
           + said + R"("}]}]},"parent_tool_use_id":)" + parent
           + R"(,"session_id":"79275283"})" + "\n";
}

std::string runSubagent(const std::string& id, const std::string& description)
{
    return makeAgentCall(id, description, "null") + makeSubagentPrompt(id)
           + makeSubagentThinking(id) + makeSubagentBash(id, id + "-bash")
           + makeSubagentBashResult(id, id + "-bash")
           + makeSubagentText(id, "Transcript.h holds the parser.");
}

std::vector<TranscriptEntryKind>
    getKinds(const std::vector<TranscriptEntry>& entries)
{
    auto kinds = std::vector<TranscriptEntryKind> {};

    for (const auto& entry: entries)
        kinds.push_back(entry.kind);

    return kinds;
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

auto tTranscriptSkips = test(
    "Transcript/orphaned subagent messages, synthetic ones and noise are left out") =
    []
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

auto tTranscriptLayoutQueued =
    test("Transcript/an open turn counts the messages queued behind it") = []
{
    constexpr auto queuedMessageCount = 2;
    auto open = makeTranscript(prompt + toolCall);
    auto rows = layoutTranscript(open, wideWidth, {}, queuedMessageCount);
    using Style = TranscriptRowStyle;

    check(rows.back().style == Style::queuedCount);
    check(rows.back().lead + rows.back().text == "⋯ 2 queued");
    check(rows[rows.size() - 2].style == Style::working);
    check(layoutTranscript(open, wideWidth).back().style == Style::working);

    auto closed = makeTranscript(prompt + answer + result);
    check(layoutTranscript(closed, wideWidth, {}, queuedMessageCount).back().style
          == Style::result);
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

auto tTranscriptQueuedPromptWithImage =
    test("Transcript/a queued prompt with a pasted image matches its replay") = []
{
    auto typed = std::string {"see ![image](.diffview/images/chat-1-1.png)"};
    auto transcript = makeTranscript(prompt);
    transcript.addQueuedPrompt(typed);

    transcript.append(
        R"json({"type":"user","message":{"role":"user","content":[{"type":"text","text":"see ![image](.diffview/images/chat-1-1.png)"},{"type":"image","source":{"type":"base64","media_type":"image/png","data":"iVBO"}}]}})json"
        + std::string {"\n"});
    check(transcript.getQueuedPrompts().empty());
    check(transcript.getEntries().back().text == typed);
};

// As Claude Code 2.1.303 wrote them: posted to an idle session, and mid-turn.
auto tTranscriptPeerPrompt =
    test("Transcript/a message posted to its inbox is a prompt, unwrapped") = []
{
    auto startedTurn = std::string {
        R"({"type":"user","message":{"role":"user","content":"Another Claude session sent a message:\nReply with PINEAPPLE.\n\nThis came from another Claude session \u2014 not typed by your user."},"parent_tool_use_id":null,"isReplay":true,"isSynthetic":true,"origin":{"kind":"peer","from":"unknown","verifiedPeerPid":43892}})"
        "\n"};
    auto midTurn = std::string {
        R"({"type":"user","message":{"role":"user","content":"Also say MANGO."},"parent_tool_use_id":null,"isReplay":true,"isSynthetic":true,"origin":{"kind":"peer","from":"unknown","verifiedPeerPid":46462}})"
        "\n"};
    auto transcript = makeTranscript({});
    transcript.addQueuedPrompt("Also say MANGO.");
    transcript.append(startedTurn + toolCall + midTurn);
    const auto& entries = transcript.getEntries();

    check(entries.size() == 3);
    check(entries[0].kind == TranscriptEntryKind::prompt
          && entries[0].text == "Reply with PINEAPPLE.");
    check(entries[2].kind == TranscriptEntryKind::prompt
          && entries[2].text == "Also say MANGO.");
    check(transcript.getQueuedPrompts().empty());
};

auto tTranscriptSentPrompt =
    test("Transcript/a prompt sent to another agent shows at once") = []
{
    auto transcript = makeTranscript(prompt);
    transcript.addSentPrompt("@ops rebase on main ");

    check(transcript.getEntries().back().kind == TranscriptEntryKind::prompt);
    check(transcript.getEntries().back().text == "@ops rebase on main");
};

auto tTranscriptSendingSpinner =
    test("Transcript/a prompt sent between turns spins until the CLI takes it") = []
{
    auto transcript = makeTranscript(prompt + result);
    transcript.addQueuedPrompt("are you there?");

    auto rows = layoutTranscript(transcript, wideWidth);
    check(rows.back().style == TranscriptRowStyle::working);
    check(rows.back().isInFlight);
    check(rows.back().text == "Sending…");
    check(rows[rows.size() - 3].style == TranscriptRowStyle::queuedPrompt);

    transcript.append(
        R"({"type":"user","message":{"role":"user","content":[{"text":"are you there?","type":"text"}]},"isReplay":true})"
        + std::string {"\n"});
    check(layoutTranscript(transcript, wideWidth).back().text == "Working…");
};

auto tTranscriptWithdrawnPrompt =
    test("Transcript/a withdrawn queued prompt leaves the pane") = []
{
    auto transcript = makeTranscript(prompt + result);
    transcript.addQueuedPrompt("never sent");
    transcript.withdrawQueuedPrompt("never sent");

    check(transcript.getQueuedPrompts().empty());
    check(layoutTranscript(transcript, wideWidth).back().style
          != TranscriptRowStyle::working);
};

auto tTranscriptSubagentNested =
    test("Transcript/a subagent's run nests under its Agent call") = []
{
    auto transcript = makeTranscript(
        prompt + runSubagent("toolu_A", "Read the parser")
        + makeToolResult(
            "toolu_A", "Transcript.h holds the parser.\nagentId: a1", "null"));
    const auto& entries = transcript.getEntries();
    using Kind = TranscriptEntryKind;

    check(getKinds(transcript) == std::vector {Kind::prompt, Kind::toolCall});

    const auto& call = entries[1];
    check(call.text == "Agent(Read the parser)");
    check(getKinds(call.subagentEntries)
          == std::vector {Kind::prompt, Kind::thinking, Kind::toolCall, Kind::text});
    check(call.subagentEntries[0].text == "Read CLAUDE.md first.");
    check(call.subagentEntries[2].text == "Bash(cat Lib/DiffView/Transcript.h)");
    check(call.subagentEntries[2].outcome
          && call.subagentEntries[2].outcome->firstLine == "#pragma once");
    check(call.subagentEntries[3].text == "Transcript.h holds the parser.");
};

auto tTranscriptSubagentRunning =
    test("Transcript/a running subagent spins its call and shows no result yet") = []
{
    auto rows = layoutTranscript(
        makeTranscript(prompt + runSubagent("toolu_A", "Read the parser")),
        wideWidth,
        {"toolu_A"});
    using Style = TranscriptRowStyle;

    check(rows[2].style == Style::toolCall && rows[2].isInFlight);
    check(rows[2].subagentToolUseId == "toolu_A");
    check(rows[3].style == Style::prompt && rows[3].indent == "⎿ ");
    check(rows[5].style == Style::toolCall && rows[5].indent == "  ");
    check(rows[5].isInFlight == false && rows[6].style == Style::toolOutcome);
    check(rows[7].style == Style::text && rows[7].indent == "  ");
    check(rows[8].style == Style::blank && rows[9].style == Style::working);
};

auto tTranscriptBackgroundSubagent =
    test("Transcript/a background subagent runs past its launch result") = []
{
    auto started = std::string {
        R"({"type":"system","subtype":"task_started","task_id":"acee39ba847179220","tool_use_id":"toolu_A","description":"Read the parser","subagent_type":"general-purpose","is_backgrounded":true,"spawn_depth":1,"task_type":"local_agent","prompt":"Go."})"
        "\n"};
    auto launched =
        makeToolResult("toolu_A", "Async agent launched successfully.", "null");
    auto finished = std::string {
        R"({"type":"system","subtype":"task_notification","task_id":"acee39ba847179220","tool_use_id":"toolu_A","status":"completed","output_file":"","summary":"Transcript.h holds the parser."})"
        "\n"};
    auto transcript = makeTranscript(
        prompt + makeAgentCall("toolu_A", "Read the parser", "null") + started
        + launched + result + makeSubagentText("toolu_A", "Reading."));

    check(!transcript.isTurnOpen() && transcript.hasRunningSubagent());

    auto rows = layoutTranscript(transcript, wideWidth);
    check(rows[2].style == TranscriptRowStyle::toolCall && rows[2].isInFlight);
    check(rows[3].style == TranscriptRowStyle::text && rows[3].text == "Reading.");
    check(rows[4].style == TranscriptRowStyle::blank);

    transcript.append(finished);
    check(!transcript.hasRunningSubagent());
    check(transcript.getEntries()[1].outcome->firstLine
          == "Transcript.h holds the parser.");
};

auto tTranscriptSubagentResult =
    test("Transcript/a finished subagent's last text is its call's result") = []
{
    auto transcript = makeTranscript(
        prompt + runSubagent("toolu_A", "Read the parser")
        + makeToolResult("toolu_A",
                         "Transcript.h holds the parser.\nagentId: a1\n<usage>",
                         "null")
        + result);
    auto rows = layoutTranscript(transcript, wideWidth);
    using Style = TranscriptRowStyle;

    check(!rows[2].isInFlight);
    check(rows[7].style == Style::toolOutcome && rows[7].indent.empty());
    check(rows[7].text == "Transcript.h holds the parser."
          && rows[7].detail.empty());
};

auto tTranscriptSubagentCollapsed =
    test("Transcript/a subagent block shows its last rows until za expands it") = []
{
    auto transcript =
        makeTranscript(prompt + runSubagent("toolu_A", "Read the parser")
                       + makeToolResult("toolu_A", "done", "null"));
    auto collapsed = layoutTranscript(transcript, wideWidth);
    auto expanded = layoutTranscript(transcript, wideWidth, {"toolu_A"});
    using Style = TranscriptRowStyle;

    check(expanded.size() == 11);
    check(collapsed.size() == 10);
    check(collapsed[3].style == Style::hiddenRows && collapsed[3].indent == "⎿ ");
    check(collapsed[3].lead == "⋯ " && collapsed[3].text == "2 more rows");
    check(collapsed[4].style == Style::toolCall && collapsed[4].indent == "  ");
    check(collapsed[4].text == "Bash(cat Lib/DiffView/Transcript.h)");
    check(collapsed[5].style == Style::toolOutcome);
    check(collapsed[6].style == Style::text);

    auto shortRun =
        makeTranscript(prompt + makeAgentCall("toolu_B", "Quick look", "null")
                       + makeSubagentText("toolu_B", "Nothing there."));
    auto shortRows = layoutTranscript(shortRun, wideWidth);
    check(shortRows[3].style == Style::text && shortRows[3].indent == "⎿ ");
};

auto tTranscriptSubagentTwoLevels =
    test("Transcript/a subagent's own subagent nests one level further") = []
{
    auto stream = prompt + runSubagent("toolu_A", "Outer")
                  + makeAgentCall("toolu_B", "Inner", quoted("toolu_A"))
                  + makeSubagentBash("toolu_B", "toolu_B-bash")
                  + makeSubagentBashResult("toolu_B", "toolu_B-bash")
                  + makeSubagentText("toolu_B", "Inner found it.")
                  + makeToolResult("toolu_B", "Inner found it.", quoted("toolu_A"));
    auto transcript = makeTranscript(stream);
    const auto& outer = transcript.getEntries()[1];
    using Kind = TranscriptEntryKind;

    check(outer.subagentEntries.back().kind == Kind::toolCall);
    const auto& inner = outer.subagentEntries.back();
    check(inner.text == "Agent(Inner)");
    check(getKinds(inner.subagentEntries)
          == std::vector {Kind::toolCall, Kind::text});
    check(inner.subagentEntries[0].outcome.has_value());

    auto rows = layoutTranscript(transcript, wideWidth, {"toolu_A", "toolu_B"});
    auto innerCall = std::find_if(rows.begin(),
                                  rows.end(),
                                  [](const TranscriptRow& row)
                                  { return row.subagentToolUseId == "toolu_B"; });
    check(innerCall != rows.end() && innerCall->indent == "  ");
    check((innerCall + 1)->indent == "  ⎿ ");
    check((innerCall + 2)->indent == "    ");
    check((innerCall + 3)->indent == "    "
          && (innerCall + 3)->text == "Inner found it.");
    check((innerCall + 4)->indent == "  "
          && (innerCall + 4)->style == TranscriptRowStyle::toolOutcome);

    for (const auto& row:
         layoutTranscript(transcript, narrowWidth, {"toolu_A", "toolu_B"}))
        check(codepointCount(row.indent + row.lead + row.text + row.detail)
              <= narrowWidth);
};

auto tTranscriptForkedCommand =
    test("Transcript/a forked skill from the prompt is a finished Skill call") = []
{
    auto parent = std::string {"forked-command-review"};
    auto stream = prompt + makeSubagentPrompt(parent) + makeSubagentThinking(parent)
                  + makeSubagentText(parent, "Looks good.") + result;
    auto transcript = makeTranscript(stream);
    const auto& entries = transcript.getEntries();

    check(entries.size() == 3);
    check(entries[1].kind == TranscriptEntryKind::toolCall);
    check(entries[1].text == "Skill(review)");
    check(entries[1].subagentEntries.size() == 3);

    auto rows = layoutTranscript(transcript, wideWidth);
    check(rows[2].style == TranscriptRowStyle::toolCall && !rows[2].isInFlight);
    check(rows[6].style == TranscriptRowStyle::toolOutcome
          && rows[6].text == "Looks good.");
};
