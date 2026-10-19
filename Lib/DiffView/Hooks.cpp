#include "Hooks.h"
#include "StreamEvents.h"

namespace diffview
{
namespace
{
using Miro::Json::Array;
using Miro::Json::Object;
using Miro::Json::Value;

constexpr auto agentToolName = "Agent";
constexpr auto preToolUseEvent = "PreToolUse";
constexpr auto sessionStartEvent = "SessionStart";
constexpr auto inboundSettingKey = "crossSessionInbound";
constexpr auto acceptInbound = "accept";

std::string quoteForShell(const std::string& text)
{
    auto quoted = std::string {"'"};

    for (auto character: text)
    {
        if (character == '\'')
            quoted += "'\\''";
        else
            quoted += character;
    }

    return quoted + "'";
}

Value makeCommandHook(const std::string& command)
{
    return Value {
        Object {{"type", Value {"command"}}, {"command", Value {command}}}};
}
} // namespace

std::optional<std::string> makeBackgroundAgentHookAnswer(std::string_view payload)
{
    auto event = Miro::Json::getParsedValue(payload);

    if (getString(event, "tool_name") != agentToolName)
        return std::nullopt;

    auto input = getField(event, "tool_input");

    if (!input.isObject())
        return std::nullopt;

    input.asObject()["run_in_background"] = Value {true};
    auto output = Object {{"hookEventName", Value {preToolUseEvent}},
                          {"permissionDecision", Value {"allow"}},
                          {"updatedInput", input}};

    return Miro::Json::print(
        Value {Object {{"hookSpecificOutput", Value {output}}}});
}

std::string makeResponderSettings(const std::string& dvExecutable,
                                  const std::string& responderName)
{
    auto dv = quoteForShell(dvExecutable) + " hook ";
    auto backgroundAgent = dv + std::string {backgroundAgentHookName};
    auto registerInbox = dv + std::string {registerInboxHookName} + " --as "
                         + quoteForShell(responderName);
    auto agentMatcher =
        Object {{"matcher", Value {agentToolName}},
                {"hooks", Value {Array {makeCommandHook(backgroundAgent)}}}};
    auto sessionStart =
        Object {{"hooks", Value {Array {makeCommandHook(registerInbox)}}}};
    auto hooks = Object {{preToolUseEvent, Value {Array {Value {agentMatcher}}}},
                         {sessionStartEvent, Value {Array {Value {sessionStart}}}}};

    return Miro::Json::print(Value {Object {
        {inboundSettingKey, Value {acceptInbound}}, {"hooks", Value {hooks}}}});
}
} // namespace diffview
