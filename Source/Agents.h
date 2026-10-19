#pragma once

#include <Wake.h>

#include <map>
#include <string>
#include <vector>

namespace diffview
{
void registerAgent(const std::string& store,
                   const std::string& name,
                   const AgentSession& session);

// Still counted as listening for a while: it is acting on what woke it.
void markAgentStopped();
void unregisterAgent();

std::vector<std::string> findListeningAgents(const std::string& store);

std::map<std::string, KnownSession> readKnownSessions(const std::string& store);

void launchDetached(const WakeLaunch& launch);
} // namespace diffview
