#pragma once

#include <Store.h>

#include <string>

namespace diffview
{
struct ThreadOptions
{
    StoreLayout store;
    int id = 0;
    std::string agent;
    bool force = false;
    std::string text;
};

int runReply(const ThreadOptions& options);
int runResolve(const ThreadOptions& options);

struct ListOptions
{
    StoreLayout store;
    std::string agent;
    bool openOnly = false;
};

int runComments(const ListOptions& options);
int runShow(const StoreLayout& store, int id);
} // namespace diffview
