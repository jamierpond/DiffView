#include "UsageLog.h"
#include "Viewer.h"

#include <SessionLog/SessionLog.h>

#include <eacp/Core/Utils/FilePath.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace diffview
{
namespace
{
constexpr auto sessionLogsToKeep = std::size_t {50};
constexpr auto millisecondsPerSecond = std::int64_t {1000};

std::ofstream& getLogStream()
{
    static auto stream = std::ofstream {};
    return stream;
}

std::string formatClockTime(std::int64_t epochMs)
{
    auto seconds = static_cast<std::time_t>(epochMs / millisecondsPerSecond);
    auto local = std::tm {};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    auto text = std::ostringstream {};
    text << std::put_time(&local, "%H:%M:%S") << '.' << std::setfill('0')
         << std::setw(3) << epochMs % millisecondsPerSecond;
    return text.str();
}
} // namespace

void startUsageLog(std::string_view mode)
{
    auto logsDirectory =
        (std::filesystem::path {eacp::FilePath::appSupportDirectory().str()}
         / "logs")
            .string();
    auto error = std::error_code {};
    std::filesystem::create_directories(logsDirectory, error);

    getLogStream().open(sessionlog::getSessionLogPath(logsDirectory, nowMs()),
                        std::ios::app);
    sessionlog::deleteOldSessionLogs(logsDirectory, sessionLogsToKeep);
    logEvent("app.launch", {{"mode", mode}});
}

void logEvent(std::string_view event,
              std::initializer_list<sessionlog::Field> fields)
{
    auto& stream = getLogStream();

    if (!stream.is_open())
        return;

    stream << formatClockTime(nowMs()) << " | "
           << sessionlog::formatEventLine(event, fields) << '\n';
    stream.flush();
}

std::int64_t UsageTimer::getElapsedMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - started).count();
}
} // namespace diffview
