#include <PathFilter.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

auto tPathFilterPlain = test("PathFilter/a plain regex is searched in the path") = []
{
    auto filter = parsePathFilter(R"(\.h$)");
    check(filter.has_value());
    check(filter->matches("Lib/DiffView/Store.h"));
    check(!filter->matches("Lib/DiffView/Store.cpp"));
};

auto tPathFilterLowerIsIcase = test("PathFilter/lowercase pattern ignores case") = []
{
    auto filter = parsePathFilter("store");
    check(filter->matches("Lib/DiffView/Store.h"));
};

auto tUpperIsExact =
    test("PathFilter/an uppercase letter makes it case-sensitive") = []
{
    auto filter = parsePathFilter("Store");
    check(filter->matches("Lib/Store.h"));
    check(!filter->matches("lib/store.h"));
};

auto tPathFilterInverted = test("PathFilter/a leading ! inverts") = []
{
    auto filter = parsePathFilter("!test");
    check(filter->matches("Source/Main.cpp"));
    check(!filter->matches("Tests/StoreTests.cpp"));
};

auto tPathFilterInvalid = test("PathFilter/an invalid regex is nullopt") = []
{ check(!parsePathFilter("(unclosed").has_value()); };

auto tPathFilterEmpty = test("PathFilter/empty or a bare ! is nullopt") = []
{
    check(!parsePathFilter("").has_value());
    check(!parsePathFilter("!").has_value());
};

auto tPathFilterFilterPaths =
    test("PathFilter/filterPaths keeps matches in order") = []
{
    auto paths = std::vector<std::string> {"a.h", "b.cpp", "c.h"};
    check(filterPaths(paths, parsePathFilter(R"(\.h$)"))
          == std::vector<std::string> {"a.h", "c.h"});
    check(filterPaths(paths, std::nullopt) == paths);
};
