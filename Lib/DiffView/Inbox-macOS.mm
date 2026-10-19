#include "Inbox.h"

#include <cerrno>
#include <cstring>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace diffview
{
namespace
{
class SocketDescriptor
{
public:
    SocketDescriptor()
        : descriptor(socket(AF_UNIX, SOCK_STREAM, 0))
    {
    }

    ~SocketDescriptor()
    {
        if (descriptor >= 0)
            close(descriptor);
    }

    SocketDescriptor(const SocketDescriptor&) = delete;
    SocketDescriptor& operator=(const SocketDescriptor&) = delete;

    int get() const { return descriptor; }

private:
    int descriptor;
};

bool connectToPath(int descriptor, const std::string& path)
{
    auto address = sockaddr_un {};
    address.sun_family = AF_UNIX;

    if (path.size() >= sizeof(address.sun_path))
        return false;

    std::memcpy(address.sun_path, path.data(), path.size());
    auto* generic = reinterpret_cast<sockaddr*>(&address);
    return connect(descriptor, generic, sizeof(address)) == 0;
}

bool writeAll(int descriptor, std::string_view bytes)
{
    while (!bytes.empty())
    {
        auto written = write(descriptor, bytes.data(), bytes.size());

        if (written < 0 && errno == EINTR)
            continue;

        if (written <= 0)
            return false;

        bytes.remove_prefix(static_cast<std::size_t>(written));
    }

    return true;
}
} // namespace

bool postToInbox(const InboxAddress& address, const std::string& text)
{
    if (!hasInboxSocket(address))
        return false;

    auto connection = SocketDescriptor {};

    if (connection.get() < 0)
        return false;

    // A listener gone mid-write is a failed post, not a SIGPIPE.
    auto isOn = 1;
    setsockopt(connection.get(), SOL_SOCKET, SO_NOSIGPIPE, &isOn, sizeof(isOn));

    if (!connectToPath(connection.get(), address.socketPath))
        return false;

    if (!writeAll(connection.get(), frameInboxMessage(address, text)))
        return false;

    shutdown(connection.get(), SHUT_WR);
    return true;
}
} // namespace diffview
