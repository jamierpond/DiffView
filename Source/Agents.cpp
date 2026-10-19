#include "Agents.h"

#include <Review.h>
#include <Store.h>

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <span>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace diffview
{
namespace
{
auto registered = std::array<char, 4096> {};

// An agent between `dv monitor --once` runs is still working its threads.
constexpr auto listeningGraceSeconds = std::time_t {600};

bool isAlive(pid_t pid)
{
    constexpr auto checkOnly = 0;
    return pid > 0 && (kill(pid, checkOnly) == 0 || errno == EPERM);
}

// Best effort: an agents directory dv can't write to mustn't stop it.
void writeRegistration(const eacp::FilePath& path, const std::string& text)
{
    auto bytes =
        std::span {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};

    try
    {
        eacp::Files::writeFile(path, bytes);
    }
    catch (const std::runtime_error&)
    {
    }
}

bool isListening(const eacp::FilePath& registration)
{
    auto in = std::istringstream {eacp::Files::readFile(registration)};
    auto word = std::string {};
    auto pid = pid_t {0};

    if (!(in >> word >> pid) || word != "pid")
        return false;

    if (isAlive(pid))
        return true;

    auto stopped = std::time_t {0};

    if (!(in >> word >> stopped) || word != "stopped")
        return false;

    return std::time(nullptr) - stopped < listeningGraceSeconds;
}
} // namespace

void registerAgent(const std::string& store, const std::string& name)
{
    createStoreDirectory(store);

    auto dir = eacp::FilePath {store} / agentDirectory;
    eacp::Files::createDirectories(dir);

    auto path = (dir / name).str();
    std::snprintf(registered.data(), registered.size(), "%s", path.c_str());

    auto text = std::ostringstream {};
    text << "pid " << getpid() << "\nstarted "
         << roundId(static_cast<std::int64_t>(std::time(nullptr)) * 1000) << "\n";
    writeRegistration(path, text.str());

    for (auto signal: {SIGINT, SIGTERM, SIGHUP, SIGPIPE})
        std::signal(signal,
                    [](int caught)
                    {
                        if (registered[0] != 0)
                            unlink(registered.data());

                        _exit(128 + caught);
                    });
}

void markAgentStopped()
{
    if (registered[0] == 0)
        return;

    auto text = std::ostringstream {};
    text << "pid 0\nstopped " << std::time(nullptr) << "\n";
    writeRegistration(registered.data(), text.str());
    registered[0] = 0;
}

std::vector<std::string> findListeningAgents(const std::string& store)
{
    auto names = std::vector<std::string> {};
    auto dir = eacp::FilePath {store} / agentDirectory;

    for (const auto& entry: eacp::Files::listDirectory(dir, {.includeHidden = true}))
    {
        if (isListening(entry.path))
            names.push_back(entry.path.fileName());
    }

    std::sort(names.begin(), names.end());
    return names;
}
} // namespace diffview
