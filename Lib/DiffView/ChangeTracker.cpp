#include "ChangeTracker.h"

#include <set>

namespace diffview
{
void ChangeTracker::update(const std::vector<FileDiff>& files, std::int64_t nowMs)
{
    auto present = std::set<std::string> {};

    for (const auto& file: files)
    {
        present.insert(file.path);
        auto [it, inserted] = entries.try_emplace(file.path);
        auto& entry = it->second;

        if (!inserted && entry.fingerprint == file.fingerprint)
            continue;

        entry.fingerprint = file.fingerprint;

        if (primed)
            entry.touchedAt = nowMs;
    }

    // A file that left the diff (reverted, committed) forgets it was seen, so
    // coming back is news.
    std::erase_if(entries,
                  [&](const auto& item) { return !present.contains(item.first); });

    primed = true;
}

void ChangeTracker::markSeen(const std::string& path)
{
    auto it = entries.find(path);

    if (it == entries.end())
        return;

    it->second.seen = true;
    it->second.seenFingerprint = it->second.fingerprint;
}

bool ChangeTracker::isUnseen(const std::string& path) const
{
    auto it = entries.find(path);

    if (it == entries.end())
        return false;

    return !it->second.seen || it->second.seenFingerprint != it->second.fingerprint;
}

bool ChangeTracker::isRecent(const std::string& path,
                             std::int64_t nowMs,
                             std::int64_t windowMs) const
{
    auto it = entries.find(path);
    return it != entries.end() && it->second.touchedAt > 0
           && nowMs - it->second.touchedAt < windowMs;
}

bool ChangeTracker::anyRecent(std::int64_t nowMs, std::int64_t windowMs) const
{
    for (const auto& [path, entry]: entries)
        if (entry.touchedAt > 0 && nowMs - entry.touchedAt < windowMs)
            return true;

    return false;
}

int ChangeTracker::unseenCount() const
{
    auto count = 0;

    for (const auto& [path, entry]: entries)
        if (isUnseen(path))
            ++count;

    return count;
}
} // namespace diffview
