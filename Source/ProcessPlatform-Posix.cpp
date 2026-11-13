#include "ProcessPlatform.h"

#include <cerrno>
#include <csignal>
#include <unistd.h>

namespace diffview
{
namespace
{
constexpr auto shellExitCodeBaseForSignals = 128;

auto terminationHandler = static_cast<void (*)(int)>(nullptr);
} // namespace

ProcessId getCurrentProcessId()
{
    return getpid();
}

bool isProcessRunning(ProcessId id)
{
    constexpr auto checkOnly = 0;
    auto posixId = static_cast<pid_t>(id);
    return id > 0 && (kill(posixId, checkOnly) == 0 || errno == EPERM);
}

bool isStandardInputTerminal()
{
    return isatty(STDIN_FILENO) != 0;
}

bool isStandardOutputTerminal()
{
    return isatty(STDOUT_FILENO) != 0;
}

void callOnTermination(void (&handler)(int exitCode))
{
    terminationHandler = &handler;

    for (auto signal: {SIGINT, SIGTERM, SIGHUP, SIGPIPE})
        std::signal(signal,
                    [](int caught)
                    { terminationHandler(shellExitCodeBaseForSignals + caught); });
}

void deleteFileFromTerminationHandler(const SignalSafePath& path)
{
    unlink(path.data());
}

void exitImmediately(int exitCode)
{
    _exit(exitCode);
}
} // namespace diffview
