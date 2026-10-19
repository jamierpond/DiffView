#include "Listener.h"
#include "Text.h"

#include <sstream>

namespace diffview
{
namespace
{
constexpr auto monitorPidKey = std::string_view {"pid"};
constexpr auto agentPidKey = std::string_view {"agent-pid"};
constexpr auto startedKey = std::string_view {"started"};
constexpr auto busyWord = std::string_view {"busy"};
constexpr auto responderWord = std::string_view {"responder"};
} // namespace

std::string formatListenerRegistration(const ListenerRegistration& registration)
{
    auto text = std::string {monitorPidKey} + " "
                + std::to_string(registration.monitorPid) + "\n";

    if (registration.agentPid > 0)
        text += std::string {agentPidKey} + " "
                + std::to_string(registration.agentPid) + "\n";

    text += std::string {startedKey} + " " + registration.started + "\n";

    if (registration.isBusy)
        text += std::string {busyWord} + "\n";

    if (registration.isResponder)
        text += std::string {responderWord} + "\n";

    return text;
}

ListenerRegistration parseListenerRegistration(const std::string& text)
{
    auto registration = ListenerRegistration {};

    for (const auto& line: splitLines(text))
    {
        auto in = std::istringstream {line};
        auto key = std::string {};
        in >> key;

        if (key == monitorPidKey)
            in >> registration.monitorPid;
        else if (key == agentPidKey)
            in >> registration.agentPid;
        else if (key == startedKey)
            in >> registration.started;
        else if (key == busyWord)
            registration.isBusy = true;
        else if (key == responderWord)
            registration.isResponder = true;
    }

    return registration;
}

std::optional<int> findLiveListenerPid(const ListenerRegistration& registration,
                                       const std::function<bool(int)>& isAlive)
{
    if (!registration.isBusy && registration.monitorPid > 0
        && isAlive(registration.monitorPid))
        return registration.monitorPid;

    if (registration.isBusy && registration.agentPid > 0
        && isAlive(registration.agentPid))
        return registration.agentPid;

    return std::nullopt;
}
} // namespace diffview
