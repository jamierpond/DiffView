#include <Hooks.h>
#include <StreamEvents.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
const auto foregroundAgentCall = std::string {
    R"({"session_id":"s1","hook_event_name":"PreToolUse","tool_name":"Agent",)"
    R"("tool_input":{"description":"Fix it","prompt":"Fix the bug",)"
    R"("subagent_type":"general-purpose","run_in_background":false},)"
    R"("tool_use_id":"toolu_1"})"};
} // namespace

auto tHookBackgroundsAgent =
    test("Hooks/an Agent call is allowed with run_in_background forced on") = []
{
    auto answer = makeBackgroundAgentHookAnswer(foregroundAgentCall);
    check(answer.has_value());

    auto output =
        getField(Miro::Json::getParsedValue(*answer), "hookSpecificOutput");
    check(getString(output, "hookEventName") == "PreToolUse");
    check(getString(output, "permissionDecision") == "allow");

    const auto& input = getField(output, "updatedInput");
    check(getString(input, "description") == "Fix it");
    check(getString(input, "prompt") == "Fix the bug");
    check(getString(input, "subagent_type") == "general-purpose");
    check(getField(input, "run_in_background").isBool()
          && getField(input, "run_in_background").asBool());
    check(input.asObject().size() == 4);
};

auto tHookAddsBackground =
    test("Hooks/an Agent call without run_in_background gets it") = []
{
    auto answer = makeBackgroundAgentHookAnswer(
        R"({"tool_name":"Agent","tool_input":{"prompt":"p"}})");
    check(answer.has_value());

    auto input =
        getField(getField(Miro::Json::getParsedValue(*answer), "hookSpecificOutput"),
                 "updatedInput");
    check(getString(input, "prompt") == "p");
    check(getField(input, "run_in_background").asBool());
};

auto tHookIgnoresOtherTools =
    test("Hooks/any other tool, or no payload, gets no answer") = []
{
    check(!makeBackgroundAgentHookAnswer(
        R"({"tool_name":"Bash","tool_input":{"command":"ls"}})"));
    check(!makeBackgroundAgentHookAnswer(R"({"tool_name":"Agent"})"));
    check(!makeBackgroundAgentHookAnswer("not json"));
    check(!makeBackgroundAgentHookAnswer(""));
};

auto tHookSettings =
    test("Hooks/the settings run dv's hook before each Agent call") = []
{
    check(
        makeBackgroundAgentHookSettings("/Users/me/.local/bin/dv")
        == R"({"hooks":{"PreToolUse":[{"hooks":[{"command":"'/Users/me/.local/bin/dv' hook background-agent","type":"command"}],"matcher":"Agent"}]}})");
    check(makeBackgroundAgentHookSettings("/a b/it's dv")
              .find(R"('/a b/it'\\''s dv' hook background-agent)")
          != std::string::npos);
};
