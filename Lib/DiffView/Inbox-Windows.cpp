#include "Inbox.h"

#include <cstdio>
#include <cstdlib>

namespace diffview
{
bool postToInbox(const InboxAddress&, const std::string&)
{
    std::fputs("dv: posting to an inbox is not implemented on Windows yet\n",
               stderr);
    std::abort();
}
} // namespace diffview
