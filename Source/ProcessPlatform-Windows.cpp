#include "ProcessPlatform.h"

#include <eacp/Core/Utils/Files.h>
#include <eacp/Core/Utils/WinInclude.h>

#include <cstdlib>

namespace diffview
{
namespace
{
constexpr auto queryAndWaitAccess =
    DWORD {PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE};
constexpr auto doNotInheritHandle = FALSE;
constexpr auto checkWithoutWaiting = DWORD {0};
constexpr auto addHandler = TRUE;
constexpr auto handledControlEvent = TRUE;

// What Windows itself reports for a console process ended by Ctrl+C.
constexpr auto consoleControlExitCode = DWORD {STATUS_CONTROL_C_EXIT};

auto terminationHandler = static_cast<void (*)(int)>(nullptr);

bool isConsole(DWORD standardHandle)
{
    auto mode = DWORD {0};
    return GetConsoleMode(GetStdHandle(standardHandle), &mode) != 0;
}

BOOL WINAPI handleConsoleControl(DWORD)
{
    auto exitCode = static_cast<int>(consoleControlExitCode);
    terminationHandler(exitCode);
    return handledControlEvent;
}
} // namespace

ProcessId getCurrentProcessId()
{
    return GetCurrentProcessId();
}

bool isProcessRunning(ProcessId id)
{
    if (id <= 0)
        return false;

    auto windowsId = static_cast<DWORD>(id);
    auto process = OpenProcess(queryAndWaitAccess, doNotInheritHandle, windowsId);

    if (process == nullptr)
        return GetLastError() == ERROR_ACCESS_DENIED;

    auto isRunning =
        WaitForSingleObject(process, checkWithoutWaiting) == WAIT_TIMEOUT;
    CloseHandle(process);
    return isRunning;
}

bool isStandardInputTerminal()
{
    return isConsole(STD_INPUT_HANDLE);
}

bool isStandardOutputTerminal()
{
    return isConsole(STD_OUTPUT_HANDLE);
}

void callOnTermination(void (&handler)(int exitCode))
{
    terminationHandler = &handler;
    SetConsoleCtrlHandler(handleConsoleControl, addHandler);
}

void deleteFileFromTerminationHandler(const SignalSafePath& path)
{
    eacp::Files::removeFile(eacp::FilePath {path.data()});
}

void exitImmediately(int exitCode)
{
    std::_Exit(exitCode);
}
} // namespace diffview
