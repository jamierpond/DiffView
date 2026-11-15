#include "RepoWatcher.h"
#include "UsageLog.h"

#include <eacp/Core/Threads/Timer.h>

namespace diffview
{
struct RepoWatcher::Native
{
    explicit Native(Handler handler)
        : onChange(std::move(handler))
    {
    }

    Handler onChange;
    eacp::Threads::Timer timer {[this] { onChange({}); }, eacp::Time::MS {1000}};
};

RepoWatcher::RepoWatcher(const std::string&, Handler onChange)
    : native(std::make_unique<Native>(std::move(onChange)))
{
    logEvent("watcher.start", {{"kind", "poll"}, {"started", true}});
}

RepoWatcher::~RepoWatcher() = default;
} // namespace diffview
