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

std::string makeBackgroundAgentHookSettings(const std::string& dvExecutable)
{
    auto command = quoteForShell(dvExecutable) + " hook "
                   + std::string {backgroundAgentHookName};
    auto hook = Object {{"type", Value {"command"}}, {"command", Value {command}}};
    auto matcher = Object {{"matcher", Value {agentToolName}},
                           {"hooks", Value {Array {Value {hook}}}}};
    auto hooks = Object {{preToolUseEvent, Value {Array {Value {matcher}}}}};

    return Miro::Json::print(Value {Object {{"hooks", Value {hooks}}}});
}
} // namespace diffview
