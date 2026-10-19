#pragma once

#include "Responder.h"

#include <memory>

namespace diffview
{
// Own session, stdout/stderr to files: dv's exit only closes its stdin.
std::unique_ptr<AgentProcess> startPipedAgentProcess(const AgentLaunch& launch);
} // namespace diffview
