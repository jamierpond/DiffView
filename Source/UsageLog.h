#pragma once

#include <SessionLog/EventLine.h>

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace diffview
{
// Starts this launch's log at <app support>/logs/<startEpochMs>.log, keeps the
// newest 50, and logs app.launch with `mode` (tui or the subcommand).
void startUsageLog(std::string_view mode);

// Appends `HH:MM:SS.mmm | <event> key=value ...` to the session log. Only the
// file: stdout belongs to the TUI and to agents parsing `dv` output.
void logEvent(std::string_view event,
              std::initializer_list<sessionlog::Field> fields = {});

// Times one slow step: ms= on the event it logs when stopped.
class UsageTimer
{
public:
    std::int64_t getElapsedMs() const;

private:
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
};
} // namespace diffview
