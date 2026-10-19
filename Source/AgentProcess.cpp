#include "AgentProcess.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace diffview
{
namespace
{
constexpr auto readChunkBytes = std::size_t {65536};
constexpr auto stopGracePeriod = std::chrono::milliseconds {1000};
constexpr auto stopPollInterval = std::chrono::milliseconds {10};
constexpr auto exitCodeForSignal = 128;

int decodeWaitStatus(int status)
{
    if (WIFSIGNALED(status))
        return exitCodeForSignal + WTERMSIG(status);

    return WEXITSTATUS(status);
}

void setFlag(int descriptor, int command, int getCommand, int flag)
{
    fcntl(descriptor, command, fcntl(descriptor, getCommand) | flag);
}

class PipedAgentProcess final : public AgentProcess
{
public:
    PipedAgentProcess(pid_t pidToUse, int inputToUse, int outputToUse)
        : pid(pidToUse)
        , input(inputToUse)
        , output(outputToUse)
    {
    }

    ~PipedAgentProcess() override { stop(); }

    int getProcessId() const override { return static_cast<int>(pid); }

    bool writeInput(std::string_view text) override
    {
        while (!text.empty() && input >= 0)
        {
            auto written = write(input, text.data(), text.size());

            if (written < 0 && errno == EINTR)
                continue;

            if (written < 0)
                return false;

            text.remove_prefix(static_cast<std::size_t>(written));
        }

        return text.empty();
    }

    std::string readAvailableOutput() override
    {
        auto text = std::string {};
        auto buffer = std::array<char, readChunkBytes> {};

        while (output >= 0)
        {
            auto count = read(output, buffer.data(), buffer.size());

            if (count <= 0)
                break;

            text.append(buffer.data(), static_cast<std::size_t>(count));
        }

        return text;
    }

    std::optional<int> findExitCode() override
    {
        if (exitCode || pid <= 0)
            return exitCode;

        auto status = 0;

        if (waitpid(pid, &status, WNOHANG) == pid)
            exitCode = decodeWaitStatus(status);

        return exitCode;
    }

    void stop() override
    {
        closeDescriptor(input);

        if (!findExitCode())
            killpg(pid, SIGTERM);

        auto deadline = std::chrono::steady_clock::now() + stopGracePeriod;

        while (!findExitCode() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(stopPollInterval);

        if (!exitCode)
        {
            killpg(pid, SIGKILL);
            auto status = 0;
            waitpid(pid, &status, 0);
            exitCode = decodeWaitStatus(status);
        }

        closeDescriptor(output);
    }

private:
    static void closeDescriptor(int& descriptor)
    {
        if (descriptor >= 0)
            close(descriptor);

        descriptor = -1;
    }

    pid_t pid = 0;
    int input = -1;
    int output = -1;
    std::optional<int> exitCode;
};
} // namespace

std::unique_ptr<AgentProcess> startPipedAgentProcess(const AgentLaunch& launch)
{
    auto toChild = std::array<int, 2> {};
    auto fromChild = std::array<int, 2> {};

    if (pipe(toChild.data()) != 0)
        return nullptr;

    if (pipe(fromChild.data()) != 0)
    {
        close(toChild[0]);
        close(toChild[1]);
        return nullptr;
    }

    for (auto descriptor: {toChild[1], fromChild[0]})
        setFlag(descriptor, F_SETFD, F_GETFD, FD_CLOEXEC);

    setFlag(fromChild[0], F_SETFL, F_GETFL, O_NONBLOCK);

    auto actions = posix_spawn_file_actions_t {};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fromChild[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions,
                                     STDERR_FILENO,
                                     launch.logPath.c_str(),
                                     O_WRONLY | O_CREAT | O_APPEND,
                                     0644);
    posix_spawn_file_actions_addchdir_np(&actions, launch.workingDirectory.c_str());

    auto attributes = posix_spawnattr_t {};
    posix_spawnattr_init(&attributes);
    short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF;
#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
    flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
    posix_spawnattr_setflags(&attributes, flags);
    posix_spawnattr_setpgroup(&attributes, 0);

    auto defaults = sigset_t {};
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    posix_spawnattr_setsigdefault(&attributes, &defaults);

    auto arguments = std::vector<char*> {};

    for (const auto& argument: launch.arguments)
        arguments.push_back(const_cast<char*>(argument.c_str()));

    arguments.push_back(nullptr);

    auto pid = pid_t {0};
    auto result = posix_spawnp(
        &pid, arguments[0], &actions, &attributes, arguments.data(), environ);

    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    close(toChild[0]);
    close(fromChild[1]);

    if (result != 0)
    {
        close(toChild[1]);
        close(fromChild[0]);
        return nullptr;
    }

    return std::make_unique<PipedAgentProcess>(pid, toChild[1], fromChild[0]);
}
} // namespace diffview
