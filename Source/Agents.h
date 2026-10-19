#pragma once

#include <Activity.h>
#include <Inbox.h>
#include <Responder.h>

#include <eacp/Core/Utils/FilePath.h>

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
// For `dv monitor --as`: listed while it runs, and after a `--once` exit as
// busy while the agent CLI process it runs under lives. `inbox` is that
// process's, from the environment it gives its Bash commands.
void registerAgent(const std::string& store,
                   const std::string& name,
                   std::optional<int> agentPid,
                   const InboxAddress& inbox);
void markAgentBusy();

// For the responder: listed while this process lives, then while its agent
// CLI process finishes.
void registerResponder(const std::string& store,
                       const std::string& name,
                       std::optional<int> agentPid);
void unregisterResponder(const std::string& store, const std::string& name);

// For the responder's session-start hook: its CLI process and that process's
// inbox, into the registration dv wrote. False when there is none.
bool recordAgentInbox(const std::string& store,
                      const std::string& name,
                      std::optional<int> agentPid,
                      const InboxAddress& inbox);

// The inbox registered as `name`'s, when it is the CLI process at agentPid's.
std::optional<InboxAddress> findAgentProcessInbox(const std::string& store,
                                                  const std::string& name,
                                                  int agentPid);

bool isRunningResponder(const std::string& store, const std::string& name);

// Its dv has quit and its agent CLI is still finishing.
bool isFinishingResponder(const std::string& store, const std::string& name);

std::vector<std::string> findListeningAgents(const std::string& store);
std::vector<ListeningAgent> listListeningAgentsWithInboxes(const std::string& store);

// The live process listening as `name`.
std::optional<int> findListenerProcessId(const std::string& store,
                                         const std::string& name);

// What listening agents said with `dv status`.
std::vector<AgentActivity> findStatusActivities(const eacp::FilePath& statuses,
                                                std::time_t now);
} // namespace diffview
