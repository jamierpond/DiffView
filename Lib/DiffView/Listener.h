#pragma once

#include "Inbox.h"

#include <functional>
#include <optional>
#include <string>

namespace diffview
{
// agents/<name>: who is listening as <name>. A `dv monitor` lists its own pid
// and the agent CLI process it runs under; after a `--once` exit the agent is
// busy acting on what woke it, and still counts as listening while its CLI
// process lives. The responder lists the UI's pid, marked as the responder,
// and its CLI's: after the UI quits, the CLI finishes on its own and still
// holds the responder until it ends. An agent CLI with an inbox socket lists
// it: dv posts to it rather than waiting on the agent.
struct ListenerRegistration
{
    int monitorPid = 0;
    int agentPid = 0;
    std::string started;
    bool isBusy = false;
    bool isResponder = false;
    // The agent CLI process's, at agentPid.
    InboxAddress inbox;
};

std::string formatListenerRegistration(const ListenerRegistration& registration);
ListenerRegistration parseListenerRegistration(const std::string& text);

// The live process that keeps it listening: the monitor, else the agent's CLI
// while it is busy or the responder's.
std::optional<int> findLiveListenerPid(const ListenerRegistration& registration,
                                       const std::function<bool(int)>& isAlive);

// A responder whose UI has quit while its CLI finishes.
bool isFinishingResponder(const ListenerRegistration& registration,
                          const std::function<bool(int)>& isAlive);
} // namespace diffview
