#pragma once

#include <AgentCli.h>

namespace diffview
{
// A live process's parent and arguments, as `ps` sees them.
std::optional<ProcessInfo> readProcessInfo(int pid);
} // namespace diffview
