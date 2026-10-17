#pragma once

#include "Diff.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace diffview
{
class ChangeTracker
{
public:
    // The first update only learns the files: nothing is recent at startup.
    void update(const std::vector<FileDiff>& files, std::int64_t nowMs);

    void markSeen(const std::string& path);

    bool isUnseen(const std::string& path) const;

    bool isRecent(const std::string& path,
                  std::int64_t nowMs,
                  std::int64_t windowMs) const;

    bool anyRecent(std::int64_t nowMs, std::int64_t windowMs) const;

    int unseenCount() const;

private:
    struct Entry
    {
        std::uint64_t fingerprint = 0;
        std::uint64_t seenFingerprint = 0;
        bool seen = false;
        std::int64_t touchedAt = 0;
    };

    std::map<std::string, Entry> entries;
    bool primed = false;
};
} // namespace diffview
