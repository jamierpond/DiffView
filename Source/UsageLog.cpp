#include "UsageLog.h"
#include "Viewer.h"

#include <PondApp/DataDirectory.h>
#include <SessionLog/SessionLog.h>

#include <eacp/Core/Utils/FilePath.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

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
// Before pond/DiffView the logs sat in a folder named after the program that
// ran: diffview, or dv through the link. Only the first found can move.
std::vector<pondapp::DataDirectoryMove>
    moveOldLogs(const std::filesystem::path& logsDirectory)
{
    const auto appDataDirectory =
        std::filesystem::path {eacp::FilePath::appDataDirectory().str()};
    auto moves = std::vector<pondapp::DataDirectoryMove> {};

    for (const auto* programName: {"diffview", "dv"})
        if (auto move = pondapp::moveOldDataDirectory(
                appDataDirectory / programName / "logs", logsDirectory))
            moves.push_back(*move);

    return moves;
}
} // namespace

void startUsageLog(std::string_view mode)
{
    const auto logsDirectoryPath = pondapp::getPondAppDataDirectory("DiffView") / "logs";
    const auto moves = moveOldLogs(logsDirectoryPath);
    const auto logsDirectory = logsDirectoryPath.string();
    auto error = std::error_code {};
    std::filesystem::create_directories(logsDirectory, error);

    getLogStream().open(sessionlog::getSessionLogPath(logsDirectory, nowMs()),
                        std::ios::app);
    sessionlog::deleteOldSessionLogs(logsDirectory, sessionLogsToKeep);
    logEvent("app.launch", {{"mode", mode}});

    for (const auto& move: moves)
        logEvent("app.migrate",
                 {{"from", move.from.string()}, {"to", move.to.string()}, {"error", move.error}});
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
