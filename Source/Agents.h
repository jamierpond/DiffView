#pragma once

#include <string>
#include <vector>

namespace diffview
{
void registerAgent(const std::string& store, const std::string& name);

// Still counted as listening for a while: it is acting on what woke it.
void markAgentStopped();

std::vector<std::string> findListeningAgents(const std::string& store);
} // namespace diffview
