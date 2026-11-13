#include "Platform.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace diffview
{
std::tm convertToLocalTime(std::time_t seconds)
{
    auto local = std::tm {};
    localtime_r(&seconds, &local);
    return local;
}

struct ExclusiveFileLock::OpenFile
{
    int descriptor = -1;
};

ExclusiveFileLock::ExclusiveFileLock(const eacp::FilePath& path)
    : file(std::make_unique<OpenFile>())
{
    file->descriptor = open(path.c_str(), O_CREAT | O_RDWR, 0644);

    if (file->descriptor >= 0)
        flock(file->descriptor, LOCK_EX);
}

ExclusiveFileLock::~ExclusiveFileLock()
{
    if (file->descriptor >= 0)
        close(file->descriptor);
}
} // namespace diffview
