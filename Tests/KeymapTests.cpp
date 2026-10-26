#include <Keymap.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

auto tSingle = test("Keymap/single keys map straight to commands") = []
{
    auto keys = KeyParser {};
    auto command = keys.feed("j");

    check(command && command->action == Action::down && command->count == 1);
};

auto tCount = test("Keymap/a count prefix repeats") = []
{
    auto keys = KeyParser {};

    check(!keys.feed("1"));
    check(!keys.feed("2"));
    check(keys.pending() == "12");

    auto command = keys.feed("k");
    check(command && command->action == Action::up && command->count == 12);
    check(keys.pending().empty());
};

auto tSequences = test("Keymap/two-key sequences") = []
{
    auto keys = KeyParser {};

    check(!keys.feed("g"));
    check(keys.feed("g")->action == Action::top);

    check(!keys.feed("]"));
    check(keys.feed("c")->action == Action::nextHunk);

    check(!keys.feed("["));
    check(keys.feed("f")->action == Action::previousFile);

    check(!keys.feed("d"));
    check(keys.feed("d")->action == Action::deleteAnnotation);

    check(!keys.feed("2"));
    check(!keys.feed("]"));
    auto command = keys.feed("a");
    check(command->action == Action::nextAnnotation && command->count == 2);
};

auto tFullContext = test("Keymap/f toggles full file context") = []
{
    auto keys = KeyParser {};

    check(keys.feed("f")->action == Action::toggleFullContext);
};

auto tBadSequence = test("Keymap/an unknown sequence clears") = []
{
    auto keys = KeyParser {};

    check(!keys.feed("g"));
    check(!keys.feed("z"));
    check(keys.pending().empty());
    check(keys.feed("j")->action == Action::down);
};

auto tEscape = test("Keymap/Esc cancels a pending sequence") = []
{
    auto keys = KeyParser {};

    keys.feed("3");
    keys.feed("d");
    check(keys.feed("Esc")->action == Action::cancel);
    check(keys.pending().empty());
};

auto tZero = test("Keymap/a leading 0 is not a count") = []
{
    auto keys = KeyParser {};

    check(!keys.feed("0"));
    check(keys.pending().empty());
};

auto tLeader = test("Keymap/space-leader pickers") = []
{
    auto keys = KeyParser {};

    check(!keys.feed(" "));
    check(!keys.feed("p"));
    check(keys.pending() == "<leader>p");
    check(keys.feed("f")->action == Action::pickFiles);

    keys.feed(" ");
    keys.feed("p");
    check(keys.feed("s")->action == Action::grepDiffs);

    keys.feed(" ");
    keys.feed("p");
    check(keys.feed("c")->action == Action::pickComments);
};

auto tUnseenAndSearch = test("Keymap/]u [u for unseen, n N for search") = []
{
    auto keys = KeyParser {};

    keys.feed("]");
    check(keys.feed("u")->action == Action::nextUnseen);
    keys.feed("[");
    check(keys.feed("u")->action == Action::previousUnseen);
    check(keys.feed("n")->action == Action::searchNext);
    check(keys.feed("N")->action == Action::searchPrevious);
    check(keys.feed("?")->action == Action::searchBackward);
    check(keys.feed("c")->action == Action::annotate);
    check(keys.feed("a")->action == Action::toggleAllFiles);
    check(!keys.feed("g"));
    check(keys.feed("h")->action == Action::annotateHunk);
    check(keys.feed("C")->action == Action::commentFile);
    check(keys.feed(":")->action == Action::commandLine);
};

auto tReply = test("Keymap/R replies on a comment's thread") = []
{
    auto keys = KeyParser {};
    auto command = keys.feed("R");

    check(command && command->action == Action::reply);
};
