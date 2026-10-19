#include "AgentProcess.h"

#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace diffview
{
namespace
{
constexpr auto exitCodeForSignal = 128;
constexpr auto appendFlags = O_WRONLY | O_CREAT | O_APPEND;
constexpr auto fileMode = mode_t {0644};

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
    PipedAgentProcess(pid_t pidToUse, int inputToUse)
        : pid(pidToUse)
        , input(inputToUse)
    {
    }

    ~PipedAgentProcess() override { closeInput(); }

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

    std::optional<int> findExitCode() override
    {
        if (exitCode || pid <= 0)
            return exitCode;

        auto status = 0;

        if (waitpid(pid, &status, WNOHANG) == pid)
            exitCode = decodeWaitStatus(status);

        return exitCode;
    }

    void closeInput() override
    {
        if (input >= 0)
            close(input);

        input = -1;
    }

private:
    pid_t pid = 0;
    int input = -1;
    std::optional<int> exitCode;
};
} // namespace

std::unique_ptr<AgentProcess> startPipedAgentProcess(const AgentLaunch& launch)
{
    auto toChild = std::array<int, 2> {};

    if (pipe(toChild.data()) != 0)
        return nullptr;

    setFlag(toChild[1], F_SETFD, F_GETFD, FD_CLOEXEC);

    auto actions = posix_spawn_file_actions_t {};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], STDIN_FILENO);
    posix_spawn_file_actions_addopen(&actions,
                                     STDOUT_FILENO,
                                     launch.outputPath.c_str(),
                                     appendFlags,
                                     fileMode);
    posix_spawn_file_actions_addopen(
        &actions, STDERR_FILENO, launch.logPath.c_str(), appendFlags, fileMode);
    posix_spawn_file_actions_addchdir_np(&actions, launch.workingDirectory.c_str());

    auto attributes = posix_spawnattr_t {};
    posix_spawnattr_init(&attributes);
    short flags = POSIX_SPAWN_SETSIGDEF;
#if defined(POSIX_SPAWN_SETSID)
    flags |= POSIX_SPAWN_SETSID;
#else
    flags |= POSIX_SPAWN_SETPGROUP;
    posix_spawnattr_setpgroup(&attributes, 0);
#endif
#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
    flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
    posix_spawnattr_setflags(&attributes, flags);

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

    if (result != 0)
    {
        close(toChild[1]);
        return nullptr;
    }

    return std::make_unique<PipedAgentProcess>(pid, toChild[1]);
}
} // namespace diffview
