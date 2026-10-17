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

std::vector<std::string> parentDirectories(const std::string& path);
} // namespace diffview
