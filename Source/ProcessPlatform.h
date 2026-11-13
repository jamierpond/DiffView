#pragma once

#include <array>
#include <cstdint>

namespace diffview
{
using ProcessId = std::int64_t;

// A path held where a termination handler can read it without allocating.
using SignalSafePath = std::array<char, 4096>;

ProcessId getCurrentProcessId();

// True for a live process, including one this user may not signal or open.
bool isProcessRunning(ProcessId id);

bool isStandardInputTerminal();
bool isStandardOutputTerminal();

// Posix: SIGINT, SIGTERM, SIGHUP and SIGPIPE, and handler runs inside the
// signal handler, so it may only make async-signal-safe calls. Windows: Ctrl+C,
// Ctrl+Break and closing the console window, on a thread of their own.
void callOnTermination(void (&handler)(int exitCode));

// Async-signal-safe on Posix, so a termination handler may call it.
void deleteFileFromTerminationHandler(const SignalSafePath& path);

// Ends the process at once: no destructors, atexit handlers or stream flushes.
[[noreturn]] void exitImmediately(int exitCode);
} // namespace diffview
