#pragma once

#include <functional>
#include <optional>
#include <string>

namespace diffview
{
// agents/<name>: who is listening as <name>. A `dv monitor` lists its own pid
// and the agent CLI process it runs under; after a `--once` exit the agent is
// busy acting on what woke it, and still counts as listening while its CLI
// process lives. The responder lists the UI's pid, marked as the responder.
struct ListenerRegistration
{
    int monitorPid = 0;
    int agentPid = 0;
    std::string started;
    bool isBusy = false;
    bool isResponder = false;
};

std::string formatListenerRegistration(const ListenerRegistration& registration);
ListenerRegistration parseListenerRegistration(const std::string& text);

// The live process that keeps it listening: the monitor, else the agent's CLI
// while it is busy.
std::optional<int> findLiveListenerPid(const ListenerRegistration& registration,
                                       const std::function<bool(int)>& isAlive);
} // namespace diffview
