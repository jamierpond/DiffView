#pragma once

#include <Inbox.h>
#include <Store.h>

#include <optional>
#include <string>

namespace diffview
{
struct MonitorOptions
{
    bool once = false;

    std::optional<std::string> after;

    std::string agent;

    // The agent CLI process it runs under.
    std::optional<int> agentPid;

    // That process's, when it has one.
    InboxAddress inbox;
};

int runMonitor(const StoreLayout& store, const MonitorOptions& options);
} // namespace diffview
