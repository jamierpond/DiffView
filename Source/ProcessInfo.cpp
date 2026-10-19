#include "ProcessInfo.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <eacp/Core/Utils/Files.h>

#include <sstream>
#endif

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace diffview
{
#if defined(__APPLE__)
namespace
{
std::optional<int> readParentPid(int pid)
{
    auto info = kinfo_proc {};
    auto size = sizeof(info);
    auto query = std::array<int, 4> {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};

    if (sysctl(query.data(), query.size(), &info, &size, nullptr, 0) != 0
        || size == 0)
        return std::nullopt;

    return info.kp_eproc.e_ppid;
}

// KERN_PROCARGS2: argc, the executable path, padding, then argc strings.
std::vector<std::string> readArguments(int pid)
{
    auto query = std::array<int, 3> {CTL_KERN, KERN_PROCARGS2, pid};
    auto size = std::size_t {0};

    if (sysctl(query.data(), query.size(), nullptr, &size, nullptr, 0) != 0)
        return {};

    auto buffer = std::vector<char>(size);

    if (sysctl(query.data(), query.size(), buffer.data(), &size, nullptr, 0) != 0
        || size < sizeof(int))
        return {};

    auto argc = 0;
    std::memcpy(&argc, buffer.data(), sizeof(argc));

    auto at = sizeof(argc);
    auto end = size;

    while (at < end && buffer[at] != '\0')
        ++at;

    while (at < end && buffer[at] == '\0')
        ++at;

    auto arguments = std::vector<std::string> {};

    while (at < end && static_cast<int>(arguments.size()) < argc)
    {
        auto argument = std::string {buffer.data() + at};
        at += argument.size() + 1;
        arguments.push_back(std::move(argument));
    }

    return arguments;
}
} // namespace

std::optional<ProcessInfo> readProcessInfo(int pid)
{
    auto parent = readParentPid(pid);

    if (!parent)
        return std::nullopt;

    return ProcessInfo {*parent, readArguments(pid)};
}
#else
// /proc/<pid>/stat: "pid (comm) state ppid ...", comm may hold spaces.
std::optional<ProcessInfo> readProcessInfo(int pid)
{
    auto directory = eacp::FilePath {"/proc/" + std::to_string(pid)};
    auto stat = eacp::Files::readFile(directory / "stat");
    auto close = stat.rfind(')');

    if (close == std::string::npos)
        return std::nullopt;

    auto in = std::istringstream {stat.substr(close + 1)};
    auto state = std::string {};
    auto parent = 0;
    in >> state >> parent;

    auto info = ProcessInfo {parent, {}};
    auto line = eacp::Files::readFile(directory / "cmdline");
    auto start = std::size_t {0};

    for (auto end = line.find('\0'); end != std::string::npos;
         end = line.find('\0', start))
    {
        info.arguments.push_back(line.substr(start, end - start));
        start = end + 1;
    }

    return info;
}
#endif
} // namespace diffview
