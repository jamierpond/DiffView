#pragma once

#include <Inbox.h>
#include <Store.h>

#include <optional>
#include <string>

namespace diffview
{
struct CommentThreadOptions
{
    StoreLayout store;
    int id = 0;
    std::string agent;
    bool force = false;
    std::string text;
};

int runReply(const CommentThreadOptions& options);
int runResolve(const CommentThreadOptions& options);

struct ListOptions
{
    StoreLayout store;
    std::string agent;
    bool openOnly = false;
};

int runComments(const ListOptions& options);

struct StatusOptions
{
    StoreLayout store;
    int id = 0;
    std::string agent;
    std::string text;
    bool isDone = false;
};

int runStatus(const StatusOptions& options);
int runShow(const StoreLayout& store, int id);

// Claude Code runs it before each Agent call: the payload is the hook's stdin.
int runBackgroundAgentHook(const std::string& payload);

// Claude Code runs it as the responder's session starts. It never fails the
// start: what goes wrong goes to stderr.
int runRegisterInboxHook(const StoreLayout& store,
                         const std::string& agent,
                         std::optional<int> agentPid,
                         const InboxAddress& inbox);
} // namespace diffview
