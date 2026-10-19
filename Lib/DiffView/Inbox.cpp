#include "Inbox.h"

#include <Miro/Json.h>

namespace diffview
{
bool hasInboxSocket(const InboxAddress& address)
{
    return !address.socketPath.empty();
}

std::string frameInboxMessage(const InboxAddress& address, const std::string& text)
{
    using Miro::Json::Object;
    using Miro::Json::Value;

    auto framed = std::string {};

    if (!address.token.empty())
    {
        auto auth =
            Object {{"type", Value {"auth"}}, {"token", Value {address.token}}};
        framed += Miro::Json::print(Value {auth}) + "\n";
    }

    auto message = Object {{"role", Value {"user"}}, {"content", Value {text}}};
    auto event = Object {{"type", Value {"user"}}, {"message", Value {message}}};

    return framed + Miro::Json::print(Value {event}) + "\n";
}
} // namespace diffview
