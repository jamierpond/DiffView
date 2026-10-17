#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace diffview
{
// On the main thread, with repo-relative paths. An empty list means
// "something may have changed": the polling fallback (non-macOS) can't say.
class RepoWatcher
{
public:
    using Handler = std::function<void(const std::vector<std::string>&)>;

    RepoWatcher(const std::string& root, Handler onChange);
    ~RepoWatcher();

    RepoWatcher(const RepoWatcher&) = delete;
    RepoWatcher& operator=(const RepoWatcher&) = delete;

private:
    struct Native;
    std::unique_ptr<Native> native;
};
} // namespace diffview
