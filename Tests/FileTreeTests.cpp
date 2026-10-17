#include <FileTree.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
const auto files = std::vector<std::string> {
    "README.md", "Lib/b.cpp", "Lib/Sub/c.h", "Lib/a.cpp", "Source/Main.cpp"};
} // namespace

auto tDirectoriesFirst = test("FileTree/directories before files, sorted") = []
{
    auto rows = buildTree(files, {});

    check(rows.size() == 8);
    check(rows[0].isDirectory && rows[0].path == "Lib" && rows[0].depth == 0);
    check(rows[1].isDirectory && rows[1].path == "Lib/Sub" && rows[1].depth == 1);
    check(rows[2].path == "Lib/Sub/c.h" && rows[2].depth == 2);
    check(rows[3].path == "Lib/a.cpp");
    check(rows[4].path == "Lib/b.cpp");
    check(rows[5].path == "Source");
    check(rows[7].path == "README.md" && rows[7].name == "README.md");
};

auto tCollapse = test("FileTree/a collapsed directory hides its children") = []
{
    auto rows = buildTree(files, {"Lib"});

    check(rows.size() == 4);
    check(rows[0].path == "Lib" && !rows[0].expanded);
    check(rows[1].path == "Source");
};

auto tOrder = test("FileTree/treeOrder walks files as the tree shows them") = []
{
    auto order = treeOrder(files);

    check(order.size() == 5);
    check(order[0] == "Lib/Sub/c.h");
    check(order[4] == "README.md");
};

auto tParents = test("FileTree/parentDirectories") = []
{
    auto parents = parentDirectories("a/b/c.txt");

    check(parents.size() == 2);
    check(parents[0] == "a" && parents[1] == "a/b");
    check(parentDirectories("top.txt").empty());
};
