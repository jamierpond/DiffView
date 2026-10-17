#include <ChangeTracker.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

auto tStartsUnseen =
    test("ChangeTracker/everything starts unseen, nothing recent") = []
{
    auto tracker = ChangeTracker {};
    tracker.update({diffForNewFile("a", "1"), diffForNewFile("b", "1")}, 1000);

    check(tracker.unseenCount() == 2);
    check(!tracker.isRecent("a", 1000, 10000));
    check(!tracker.anyRecent(1000, 10000));
};

auto tSeenUntilChanged = test("ChangeTracker/seen until the diff changes") = []
{
    auto tracker = ChangeTracker {};
    tracker.update({diffForNewFile("a", "1")}, 1000);
    tracker.markSeen("a");

    check(!tracker.isUnseen("a"));

    tracker.update({diffForNewFile("a", "1")}, 2000);
    check(!tracker.isUnseen("a"));

    tracker.update({diffForNewFile("a", "2")}, 3000);
    check(tracker.isUnseen("a"));
    check(tracker.isRecent("a", 3500, 1000));
    check(!tracker.isRecent("a", 4500, 1000));
};

auto tLeavingForgets =
    test("ChangeTracker/a file that leaves the diff comes back unseen") = []
{
    auto tracker = ChangeTracker {};
    tracker.update({diffForNewFile("a", "1")}, 1000);
    tracker.markSeen("a");
    tracker.update({}, 2000);
    tracker.update({diffForNewFile("a", "1")}, 3000);

    check(tracker.isUnseen("a"));
    check(tracker.isRecent("a", 3000, 1000));
};
