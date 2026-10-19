#pragma once

#include <Activity.h>

#include <eacp/Core/Utils/FilePath.h>

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
// For `dv monitor --as`: listed until it exits, however it exits.
void registerAgent(const std::string& store, const std::string& name);
void unregisterAgent();

// For the responder: listed while this process lives.
void registerResponder(const std::string& store, const std::string& name);
void unregisterResponder(const std::string& store, const std::string& name);

bool isRunningResponder(const std::string& store, const std::string& name);

std::vector<std::string> findListeningAgents(const std::string& store);

// The live process listening as `name`.
std::optional<int> findListenerProcessId(const std::string& store,
                                         const std::string& name);

// What listening agents said with `dv status`.
std::vector<AgentActivity> findStatusActivities(const eacp::FilePath& statuses,
                                                std::time_t now);
} // namespace diffview
