#include "Commands.h"
#include "Monitor.h"
#include "Viewer.h"

#include <Review.h>
#include <Store.h>

#include <eacp/Core/App/App.h>

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{
constexpr auto usage =
    "usage: dv [<base>] [repo]                    the review UI, for a human\n"
    "       dv <command> [args] [--repo <dir>]    the agent side\n"
    "\n"
    "Shows the working tree's diff against a base (default HEAD), live, and\n"
    "sends your comments as review rounds to <repo>/.diffview/latest.md.\n"
    "\n"
    "  dv              this repo, against HEAD (working tree + index + untracked)\n"
    "  dv main...      against where HEAD forked from main\n"
    "  dv HEAD~3       against that exact commit\n"
    "  dv ~/other      another repo\n"
    "\n"
    "It is a terminal UI for a human. F1 inside it lists the keys.\n"
    "\n"
    "Agent loop (no terminal needed):\n"
    "  monitor    wait for the next review round\n"
    "  comments   list the review threads\n"
    "  show       print one thread with its code\n"
    "  reply      answer a thread\n"
    "  resolve    close a thread\n"
    "  help       this, or `dv help <command>`\n"
    "\n"
    "For coding agents: the human watches your edits in dv and comments line by\n"
    "line, then submits a round. The loop:\n"
    "  1. run `dv monitor --once --as <you> --after <last round>` as a\n"
    "     background shell task (not a Monitor tool: it can wait for hours)\n"
    "  2. when it exits it prints the round, e.g.\n"
    "       dv: review round 2026-10-17-142300 submitted · for @you · 3 open ·\n"
    "       1 with new replies · read /abs/repo/.diffview/latest.md\n"
    "     or a reply the human sent on one thread:\n"
    "       dv: reply 2026-10-17-143512 on #4 from you · for @you · read <path>\n"
    "  3. `dv comments --as <you> --open`, `dv show <id>` for each\n"
    "  4. act, then `dv reply <id> \"what you did\"`; `dv resolve <id>` only\n"
    "     once the work is done. A question gets a reply and stays open:\n"
    "     resolving folds the thread away from the human.\n"
    "  5. re-arm with --after set to the id in that line\n"
    "\n"
    "Addressing: `@name` in a comment gives its thread to that agent; an\n"
    "unaddressed thread wakes every agent and belongs to the first to answer\n"
    "it. Another agent's thread is refused unless --force. Without --as you\n"
    "hear every round. The owner shows in the heading: `### a.cpp:12 · open ·\n"
    "@name · #7`. Rounds are Markdown in .diffview/reviews/; latest.md is the\n"
    "newest.\n";

const std::map<std::string, std::string>& commandHelp()
{
    static const auto help = std::map<std::string, std::string> {
        {"monitor",
         "Wait for review rounds; print one line per round.\n"
         "\n"
         "usage: dv monitor [--once] [--after <round>] [--as <name>] [--repo <dir>]\n"
         "\n"
         "  --once           exit after the first round\n"
         "  --after <round>  a round newer than this one counts even if it is\n"
         "                   already there (exits at once with --once)\n"
         "  --as <name>      only rounds with something for you: threads you own\n"
         "                   or nobody does, or that name you; also shows you as\n"
         "                   listening in the UI\n"
         "\n"
         "Each line names the round to pass to the next --after:\n"
         "  dv: review round 2026-10-17-142300 submitted · for @dev · 2 open · "
         "read <path>\n"
         "  dv: reply 2026-10-17-143512 on #4 from you · for @dev · read <path>\n"
         "A reply is sent the moment the human writes it, as its own round.\n"
         "\n"
         "example: dv monitor --once --as diffview-dev --after 2026-10-17-142300\n"},
        {"comments",
         "List the review threads: id, status, owner, where, first line.\n"
         "\n"
         "usage: dv comments [--open] [--as <name>] [--repo <dir>]\n"
         "\n"
         "  --open       skip resolved ones\n"
         "  --as <name>  only yours and nobody's\n"
         "\n"
         "example: dv comments --open --as diffview-dev\n"
         "  #4  open  @diffview-dev  src/Focus.cpp:48-49  Why a reference? · 1 "
         "reply, last you\n"},
        {"show",
         "Print one thread: its heading, the code it is about, the comment and\n"
         "every reply.\n"
         "\n"
         "usage: dv show <id> [--repo <dir>]\n"
         "\n"
         "example: dv show 4\n"},
        {"reply",
         "Answer a thread. The UI shows it at once. Claims an unowned thread.\n"
         "\n"
         "usage: dv reply <id> [-m] <text> [--as <name>] [--force] [--repo <dir>]\n"
         "       dv reply <id> [--as <name>] < text\n"
         "\n"
         "  --as <name>  who is answering (default: agent, which claims nothing)\n"
         "  --force      answer a thread another agent owns\n"
         "\n"
         "example: dv reply 4 --as diffview-dev \"Done: it's a copy again.\"\n"},
        {"resolve",
         "Close a thread, with an optional closing note. Only once the work is\n"
         "done: a question gets `dv reply` and stays open, since a resolved\n"
         "thread folds away from the human.\n"
         "\n"
         "usage: dv resolve <id> [[-m] <note>] [--as <name>] [--force] [--repo <dir>]\n"
         "\n"
         "example: dv resolve 4 --as diffview-dev \"Renamed in 3f2a1c.\"\n"},
        {"help",
         "usage: dv help [<command>]\n"
         "\n"
         "example: dv help reply\n"},
    };

    return help;
}

int usageError(const std::string& message, const std::string& command)
{
    std::fprintf(stderr, "dv: %s\n", message.c_str());

    if (!command.empty())
        std::fprintf(stderr, "run `dv %s --help` for its usage\n", command.c_str());
    else
        std::fputs("run `dv --help` for usage\n", stderr);

    return 2;
}

struct Arguments
{
    std::vector<std::string> positional;
    std::map<std::string, std::string> values;
    std::map<std::string, bool> flags;
    bool help = false;
};

const auto valueFlags = std::map<std::string, std::string> {
    {"--repo", "repo"},
    {"--as", "as"},
    {"--from", "as"},
    {"--after", "after"},
    {"-m", "message"},
    {"--message", "message"},
};

std::optional<std::string>
    parseArguments(int argc, char* argv[], int first, Arguments& arguments)
{
    for (auto i = first; i < argc; ++i)
    {
        auto arg = std::string {argv[i]};

        if (arg == "-h" || arg == "--help")
            arguments.help = true;
        else if (auto found = valueFlags.find(arg); found != valueFlags.end())
        {
            if (i + 1 >= argc)
                return arg + " needs a value";

            arguments.values[found->second] = argv[++i];
        }
        else if (arg == "--once" || arg == "--force" || arg == "--open")
            arguments.flags[arg] = true;
        else if (arg == "--")
        {
            for (++i; i < argc; ++i)
                arguments.positional.emplace_back(argv[i]);
        }
        else if (arg.starts_with("-") && arg.size() > 1)
            return "unknown option " + arg
                   + " (for text starting with -, use -m <text> or -- <text>)";
        else
            arguments.positional.push_back(arg);
    }

    return std::nullopt;
}

std::optional<int> parseId(const std::string& text)
{
    auto digits = text.starts_with("#") ? text.substr(1) : text;

    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos
        || digits.size() > 9)
        return std::nullopt;

    return std::stoi(digits);
}

std::optional<diffview::StoreLayout> openStore(const std::string& root)
{
    auto store = diffview::findStore(root);

    if (!store)
        std::fprintf(
            stderr, "dv: can't find the git directories of %s\n", root.c_str());

    return store;
}

void printMergeNotes(const std::vector<std::string>& notes)
{
    for (const auto& note: notes)
        std::fprintf(stderr, "dv: %s\n", note.c_str());
}

std::string readStdin()
{
    return {std::istreambuf_iterator<char> {std::cin},
            std::istreambuf_iterator<char> {}};
}

int runCommand(const std::string& command, int argc, char* argv[])
{
    auto arguments = Arguments {};

    if (auto error = parseArguments(argc, argv, 2, arguments))
        return usageError(*error, command);

    if (command == "help" || arguments.help)
    {
        auto topic = command == "help" && !arguments.positional.empty()
                         ? arguments.positional.front()
                         : command;
        auto found = commandHelp().find(topic);

        if (command == "help" && arguments.positional.empty())
            std::fputs(usage, stdout);
        else if (found == commandHelp().end())
            return usageError("no command " + topic, {});
        else
            std::fputs(found->second.c_str(), stdout);

        return 0;
    }

    auto& positional = arguments.positional;
    auto path = arguments.values.contains("repo")
                    ? arguments.values["repo"]
                    : std::filesystem::current_path().string();

    if (command == "monitor" && positional.size() == 1)
    {
        path = positional.front();
        positional.clear();
    }

    auto agent = arguments.values["as"];

    if (!agent.empty() && (!diffview::isAgentName(agent) || agent == "you"))
        return usageError("--as " + agent
                              + ": a name is letters, digits, - _ . and not `you`",
                          command);

    auto root = diffview::git::findRoot(path);

    if (!root)
    {
        std::fprintf(stderr, "dv: %s is not inside a git work tree\n", path.c_str());
        return 1;
    }

    auto store = openStore(*root);

    if (!store)
        return 1;

    printMergeNotes(diffview::mergeWorktreeStores(*store));

    if (command == "monitor")
    {
        if (!positional.empty())
            return usageError("unexpected " + positional.front(), command);

        auto options = diffview::MonitorOptions {};
        options.once = arguments.flags["--once"];
        options.agent = agent;

        if (arguments.values.contains("after"))
            options.after = arguments.values["after"];

        return diffview::runMonitor(*store, options);
    }

    if (command == "comments")
    {
        if (!positional.empty())
            return usageError("unexpected " + positional.front(), command);

        return diffview::runComments({*store, agent, arguments.flags["--open"]});
    }

    if (positional.empty())
        return usageError("which comment? give its id, e.g. `dv " + command
                              + " 4` (dv comments lists them)",
                          command);

    auto id = parseId(positional.front());

    if (!id)
        return usageError(positional.front() + " is not a comment id", command);

    positional.erase(positional.begin());

    if (command == "show")
    {
        if (!positional.empty())
            return usageError("unexpected " + positional.front(), command);

        return diffview::runShow(*store, *id);
    }

    auto text = arguments.values["message"];

    for (const auto& word: positional)
        text += (text.empty() ? "" : " ") + word;

    if (command == "reply" && text.empty() && isatty(STDIN_FILENO) == 0)
        text = readStdin();

    auto options = diffview::ThreadOptions {
        *store, *id, agent, arguments.flags["--force"], text};

    return command == "reply" ? diffview::runReply(options)
                              : diffview::runResolve(options);
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc > 1 && commandHelp().contains(argv[1]))
        return runCommand(argv[1], argc, argv);

    auto options = diffview::Options {};
    auto path = std::filesystem::current_path().string();

    for (auto i = 1; i < argc; ++i)
    {
        auto arg = std::string {argv[i]};

        if (arg == "-h" || arg == "--help")
        {
            std::fputs(usage, stdout);
            return 0;
        }

        if (arg == "--base" && i + 1 < argc)
            options.base = diffview::git::BaseSpec::parse(argv[++i]);
        else if (arg == "--repo" && i + 1 < argc)
            path = argv[++i];
        // Like `git diff main`: a positional that isn't a directory is the base.
        else if (!arg.starts_with("-") && std::filesystem::is_directory(arg))
            path = arg;
        else if (!arg.starts_with("-"))
            options.base = diffview::git::BaseSpec::parse(arg);
        else
            return usageError("unknown option " + arg, {});
    }

    auto root = diffview::git::findRoot(path);

    if (!root)
    {
        std::fprintf(
            stderr, "diffview: %s is not inside a git work tree\n", path.c_str());
        return 1;
    }

    if (isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
    {
        std::fputs("diffview: needs a terminal\n", stderr);
        return 1;
    }

    auto store = openStore(*root);

    if (!store)
        return 1;

    options.repo = *root;
    options.store = *store;
    options.mergeNotes = diffview::mergeWorktreeStores(*store);
    return eacp::Apps::run<diffview::Viewer>(options);
}
