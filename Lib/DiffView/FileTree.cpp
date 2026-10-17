#include "FileTree.h"

#include <algorithm>
#include <map>
#include <memory>

namespace diffview
{
namespace
{
struct Node
{
    std::map<std::string, std::unique_ptr<Node>> directories;
    std::vector<std::string> files;
};

Node makeTree(const std::vector<std::string>& files)
{
    auto root = Node {};

    for (const auto& path: files)
    {
        auto* node = &root;
        auto start = std::size_t {0};

        while (true)
        {
            auto slash = path.find('/', start);

            if (slash == std::string::npos)
            {
                node->files.push_back(path.substr(start));
                break;
            }

            auto& child = node->directories[path.substr(start, slash - start)];

            if (!child)
                child = std::make_unique<Node>();

            node = child.get();
            start = slash + 1;
        }
    }

    return root;
}

void flatten(const Node& node,
             const std::string& prefix,
             int depth,
             const std::set<std::string>& collapsed,
             std::vector<TreeRow>& rows)
{
    for (const auto& [name, child]: node.directories)
    {
        auto path = prefix.empty() ? name : prefix + "/" + name;
        auto expanded = !collapsed.contains(path);
        rows.push_back({name, path, depth, true, expanded});

        if (expanded)
            flatten(*child, path, depth + 1, collapsed, rows);
    }

    auto files = node.files;
    std::sort(files.begin(), files.end());

    for (const auto& name: files)
        rows.push_back(
            {name, prefix.empty() ? name : prefix + "/" + name, depth, false, true});
}
} // namespace

std::vector<TreeRow> buildTree(const std::vector<std::string>& files,
                               const std::set<std::string>& collapsed)
{
    auto rows = std::vector<TreeRow> {};
    flatten(makeTree(files), {}, 0, collapsed, rows);
    return rows;
}

std::vector<std::string> treeOrder(const std::vector<std::string>& files)
{
    auto order = std::vector<std::string> {};

    for (const auto& row: buildTree(files, {}))
        if (!row.isDirectory)
            order.push_back(row.path);

    return order;
}

std::vector<std::string> parentDirectories(const std::string& path)
{
    auto parents = std::vector<std::string> {};

    for (auto slash = path.find('/'); slash != std::string::npos;
         slash = path.find('/', slash + 1))
        parents.push_back(path.substr(0, slash));

    return parents;
}
} // namespace diffview
