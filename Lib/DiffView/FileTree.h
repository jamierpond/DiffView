#pragma once

#include <set>
#include <string>
#include <vector>

namespace diffview
{
struct TreeRow
{
    std::string name;

    std::string path;

    int depth = 0;
    bool isDirectory = false;
    bool expanded = true;
};

// Directories before files, each level sorted by name.
std::vector<TreeRow> buildTree(const std::vector<std::string>& files,
                               const std::set<std::string>& collapsed);

// What ]f / [f and ]u walk: the fully expanded tree's order.
std::vector<std::string> treeOrder(const std::vector<std::string>& files);

// The row showing `path`, or -1.
int rowOf(const std::vector<TreeRow>& rows, const std::string& path);

// Where the tree cursor goes when the rows are rebuilt under it (a refresh
// adds or drops files, a directory folds): the row it was on, found by path,
// else the selected file's row, else the same index, clamped.
int cursorAfterRebuild(const std::vector<TreeRow>& before,
                       int cursor,
                       const std::vector<TreeRow>& after,
                       const std::string& selected);

std::vector<std::string> parentDirectories(const std::string& path);
} // namespace diffview
