#pragma once

#include <eacp/Core/Utils/FilePath.h>

#include <ctime>
#include <memory>

namespace diffview
{
std::tm convertToLocalTime(std::time_t seconds);

// Waits until no other process holds the lock on path, creating the file if
// needed; the lock is released on destruction. Failing to open the file
// leaves nothing locked, as before.
class ExclusiveFileLock
{
public:
    explicit ExclusiveFileLock(const eacp::FilePath& path);
    ~ExclusiveFileLock();

    ExclusiveFileLock(const ExclusiveFileLock&) = delete;
    ExclusiveFileLock& operator=(const ExclusiveFileLock&) = delete;

private:
    struct OpenFile;
    std::unique_ptr<OpenFile> file;
};
} // namespace diffview
