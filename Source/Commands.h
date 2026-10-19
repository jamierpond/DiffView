#pragma once

#include <string>

namespace diffview
{
struct ThreadOptions
{
    std::string repo;
    int id = 0;
    std::string agent;
    bool force = false;
    std::string text;
};

int runReply(const ThreadOptions& options);
int runResolve(const ThreadOptions& options);

struct ListOptions
{
    std::string repo;
    std::string agent;
    bool openOnly = false;
};

int runComments(const ListOptions& options);
int runShow(const std::string& repo, int id);
} // namespace diffview
