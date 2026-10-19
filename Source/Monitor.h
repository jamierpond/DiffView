#pragma once

#include <optional>
#include <string>

namespace diffview
{
struct MonitorOptions
{
    bool once = false;

    std::optional<std::string> after;

    std::string agent;
};

int runMonitor(const std::string& repo, const MonitorOptions& options);
} // namespace diffview
