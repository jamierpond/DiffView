#pragma once

#include <Store.h>
#include <Wake.h>

#include <optional>
#include <string>

namespace diffview
{
struct MonitorOptions
{
    bool once = false;

    std::optional<std::string> after;

    std::string agent;

    AgentSession session;
};

int runMonitor(const StoreLayout& store, const MonitorOptions& options);
} // namespace diffview
