#pragma once

#include <Responder.h>

#include <memory>

namespace diffview
{
// Its own process group, stdin and stdout piped to us, stderr to the log. It
// can't outlive dv: when dv goes, however it goes, its stdin closes and the
// CLI ends.
std::unique_ptr<AgentProcess> startPipedAgentProcess(const AgentLaunch& launch);
} // namespace diffview
