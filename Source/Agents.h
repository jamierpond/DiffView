#pragma once

#include <string>
#include <vector>

namespace diffview
{
void registerAgent(const std::string& repo, const std::string& name);
void unregisterAgent();

std::vector<std::string> findListeningAgents(const std::string& repo);
} // namespace diffview
