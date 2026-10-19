#include "AgentProcess.h"

#include <cstdio>
#include <cstdlib>

namespace diffview
{
std::unique_ptr<AgentProcess> startPipedAgentProcess(const AgentLaunch&)
{
    std::fputs("dv: agent processes are not implemented on Windows yet\n", stderr);
    std::abort();
}
} // namespace diffview
