#pragma once

#include <functional>
#include <string>

namespace diffview
{
// A Claude Code session's inbox socket and its token, as it exports them to
// its hooks and Bash commands (CLAUDE_CODE_MESSAGING_SOCKET and _TOKEN).
struct InboxAddress
{
    std::string socketPath;
    std::string token;
};

bool hasInboxSocket(const InboxAddress& address);

// What one post writes: an auth line when there is a token, then the message
// as a stream-json user line.
std::string frameInboxMessage(const InboxAddress& address, const std::string& text);

// Connects, writes frameInboxMessage and closes. False when nothing listens
// there or the write fails; true says it was written, not that the session's
// inbound controls let it through.
bool postToInbox(const InboxAddress& address, const std::string& text);

using PostToInbox =
    std::function<bool(const InboxAddress& address, const std::string& text)>;
} // namespace diffview
