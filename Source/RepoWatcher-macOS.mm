#include "RepoWatcher.h"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include <climits>
#include <cstdlib>

namespace diffview
{
namespace
{
// FSEvents reports resolved paths (/private/var/... for /var/...), so the
// prefix to strip has to be resolved the same way.
std::string resolved(const std::string& path)
{
    char buffer[PATH_MAX];

    if (realpath(path.c_str(), buffer) == nullptr)
        return path;

    return buffer;
}
} // namespace

struct RepoWatcher::Native
{
    ~Native()
    {
        if (stream == nullptr)
            return;

        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
    }

    static void callback(ConstFSEventStreamRef,
                         void* info,
                         size_t count,
                         void* paths,
                         const FSEventStreamEventFlags*,
                         const FSEventStreamEventId*)
    {
        auto* self = static_cast<Native*>(info);
        auto* list = static_cast<char**>(paths);
        auto relative = std::vector<std::string> {};

        for (size_t i = 0; i < count; ++i)
        {
            auto path = std::string {list[i]};

            if (path.starts_with(self->prefix))
                relative.push_back(path.substr(self->prefix.size()));
            else if (path + "/" == self->prefix)
                relative.push_back({});
        }

        if (!relative.empty())
            self->onChange(relative);
    }

    std::string prefix;
    Handler onChange;
    FSEventStreamRef stream = nullptr;
};

RepoWatcher::RepoWatcher(const std::string& root, Handler onChange)
    : native(std::make_unique<Native>())
{
    native->prefix = resolved(root) + "/";
    native->onChange = std::move(onChange);

    auto context = FSEventStreamContext {0, native.get(), nullptr, nullptr, nullptr};
    auto rootString = CFStringCreateWithCString(nullptr, root.c_str(), kCFStringEncodingUTF8);
    auto roots = CFArrayCreate(nullptr, (const void**) &rootString, 1, &kCFTypeArrayCallBacks);

    native->stream = FSEventStreamCreate(nullptr,
                                         &Native::callback,
                                         &context,
                                         roots,
                                         kFSEventStreamEventIdSinceNow,
                                         0.05,
                                         kFSEventStreamCreateFlagFileEvents
                                             | kFSEventStreamCreateFlagNoDefer);
    CFRelease(roots);
    CFRelease(rootString);

    if (native->stream == nullptr)
        return;

    FSEventStreamSetDispatchQueue(native->stream, dispatch_get_main_queue());
    FSEventStreamStart(native->stream);
}

RepoWatcher::~RepoWatcher() = default;
} // namespace diffview
