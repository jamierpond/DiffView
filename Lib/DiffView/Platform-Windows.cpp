#include "Platform.h"

#include <eacp/Core/Utils/WinInclude.h>

namespace diffview
{
namespace
{
constexpr auto readAndWrite = DWORD {GENERIC_READ | GENERIC_WRITE};
constexpr auto letOthersOpenTheFile =
    DWORD {FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE};
constexpr auto waitForTheLock = DWORD {LOCKFILE_EXCLUSIVE_LOCK};
constexpr auto reservedZero = DWORD {0};
constexpr auto wholeFileLengthLow = DWORD {MAXDWORD};
constexpr auto wholeFileLengthHigh = DWORD {MAXDWORD};
} // namespace

std::tm convertToLocalTime(std::time_t seconds)
{
    auto local = std::tm {};
    localtime_s(&local, &seconds);
    return local;
}

struct ExclusiveFileLock::OpenFile
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    OVERLAPPED lockedRange {};
};

ExclusiveFileLock::ExclusiveFileLock(const eacp::FilePath& path)
    : file(std::make_unique<OpenFile>())
{
    file->handle = CreateFileW(path.wide().c_str(),
                               readAndWrite,
                               letOthersOpenTheFile,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);

    if (file->handle != INVALID_HANDLE_VALUE)
    {
        LockFileEx(file->handle,
                   waitForTheLock,
                   reservedZero,
                   wholeFileLengthLow,
                   wholeFileLengthHigh,
                   &file->lockedRange);
    }
}

ExclusiveFileLock::~ExclusiveFileLock()
{
    if (file->handle == INVALID_HANDLE_VALUE)
        return;

    UnlockFileEx(file->handle,
                 reservedZero,
                 wholeFileLengthLow,
                 wholeFileLengthHigh,
                 &file->lockedRange);
    CloseHandle(file->handle);
}
} // namespace diffview
