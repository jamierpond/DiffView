#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace telescope
{
// Smartcase. Runs, word and path-component starts and the file name part
// of a path score higher.
struct FuzzyMatch
{
    int score = 0;
    std::vector<std::size_t> positions;
};

std::optional<FuzzyMatch> fuzzyMatch(std::string_view query,
                                     std::string_view candidate);

// The query line, and any other prompt line (search, :commands).
class LineEdit
{
public:
    // False for keys it doesn't use.
    bool feed(const std::string& key);

    void set(std::string text);
    const std::string& text() const;

    std::size_t cursor() const;

private:
    std::string value;
    std::size_t position = 0;
};

struct Item
{
    std::string label;
    std::string detail;
    std::string key;
};

// With fuzzy off the host supplies already-matched items for each query (a
// live grep) and they are shown as given.
class Picker
{
public:
    void setItems(std::vector<Item> items);
    void setQuery(std::string query);
    void setFuzzy(bool fuzzy);

    const std::vector<Item>& items() const;

    const std::vector<int>& visible() const;

    std::vector<std::size_t> highlights(int row) const;

    // -1 when nothing is visible.
    int selected() const;
    void move(int delta);
    void select(int row);

    const Item* current() const;

    std::function<std::vector<std::string>(const Item&)> preview = [](const Item&)
    { return std::vector<std::string> {}; };

private:
    void refilter();

    std::vector<Item> all;
    std::vector<int> shown;
    std::string text;
    bool fuzzy = true;
    int row = 0;
};
} // namespace telescope
