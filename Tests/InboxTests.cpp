#include <Inbox.h>

#include <NanoTest/NanoTest.h>

#include <array>
#include <filesystem>
#include <string>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace nano;
using namespace diffview;

auto tInboxFrame =
    test("Inbox/a post is the auth line, then a user message line") = []
{
    check(
        frameInboxMessage({"/tmp/cc-socks/1.sock", "abc123"},
                          "Also \"check\" #4\nplease")
        == "{\"token\":\"abc123\",\"type\":\"auth\"}\n"
           "{\"message\":{\"content\":\"Also \\\"check\\\" #4\\nplease\",\"role\":\"user\"},"
           "\"type\":\"user\"}\n");
};

auto tInboxFrameWithoutToken =
    test("Inbox/without a token a post is only the message line") = []
{
    check(
        frameInboxMessage({"/tmp/cc-socks/1.sock", ""}, "hi")
        == "{\"message\":{\"content\":\"hi\",\"role\":\"user\"},\"type\":\"user\"}\n");
};

auto tInboxNoSocket = test("Inbox/posting where nothing listens fails") = []
{
    auto path = std::filesystem::temp_directory_path() / "dv-no-inbox.sock";
    std::filesystem::remove(path);

    check(!postToInbox({path.string(), "t"}, "hi"));
    check(!postToInbox({}, "hi"));
};

#if !defined(_WIN32)
namespace
{
class ListeningSocket
{
public:
    explicit ListeningSocket(const std::string& pathToUse)
        : path(pathToUse)
        , descriptor(socket(AF_UNIX, SOCK_STREAM, 0))
    {
        unlink(path.c_str());
        auto address = sockaddr_un {};
        address.sun_family = AF_UNIX;
        path.copy(address.sun_path, sizeof(address.sun_path) - 1);
        auto* generic = reinterpret_cast<sockaddr*>(&address);
        bind(descriptor, generic, sizeof(address));
        listen(descriptor, 1);
    }

    ~ListeningSocket()
    {
        close(descriptor);
        unlink(path.c_str());
    }

    ListeningSocket(const ListeningSocket&) = delete;
    ListeningSocket& operator=(const ListeningSocket&) = delete;

    // Everything the one connection wrote until it shut its side.
    std::string readOneConnection()
    {
        auto connection = accept(descriptor, nullptr, nullptr);
        auto received = std::string {};
        auto buffer = std::array<char, 256> {};

        while (true)
        {
            auto count = read(connection, buffer.data(), buffer.size());

            if (count <= 0)
                break;

            received.append(buffer.data(), static_cast<std::size_t>(count));
        }

        close(connection);
        return received;
    }

private:
    std::string path;
    int descriptor;
};
} // namespace

auto tInboxPostWritesFrame =
    test("Inbox/a post writes its frame to the socket and closes") = []
{
    auto path =
        (std::filesystem::temp_directory_path() / "dv-inbox-test.sock").string();
    auto listener = ListeningSocket {path};
    auto address = InboxAddress {path, "tok"};

    check(postToInbox(address, "round 3"));
    check(listener.readOneConnection() == frameInboxMessage(address, "round 3"));
};
#endif
