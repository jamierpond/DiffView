#include "Telescope.h"

#include <algorithm>
#include <cctype>

namespace telescope
{
namespace
{
bool ignoresCase(std::string_view query)
{
    return std::none_of(query.begin(),
                        query.end(),
                        [](char c)
                        { return std::isupper(static_cast<unsigned char>(c)); });
}

char fold(char c, bool ignoreCase)
{
    return ignoreCase
               ? static_cast<char>(std::tolower(static_cast<unsigned char>(c)))
               : c;
}

bool isBoundary(char previous)
{
    return previous == '/' || previous == '_' || previous == '-' || previous == '.'
           || previous == ' ' || previous == ':';
}
} // namespace

namespace
{
bool isContinuation(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// One code point, not a named key like "F1".
bool isCharacter(const std::string& key)
{
    if (key.empty())
        return false;

    auto lead = static_cast<unsigned char>(key[0]);

    if (key.size() == 1)
        return lead >= 0x20 && lead != 0x7F;

    if ((lead & 0xC0) != 0xC0)
        return false;

    return std::all_of(key.begin() + 1, key.end(), isContinuation);
}
} // namespace

bool LineEdit::feed(const std::string& key)
{
    auto previous = [&](std::size_t at)
    {
        if (at == 0)
            return at;

        --at;

        while (at > 0 && isContinuation(value[at]))
            --at;

        return at;
    };

    auto next = [&](std::size_t at)
    {
        if (at >= value.size())
            return value.size();

        ++at;

        while (at < value.size() && isContinuation(value[at]))
            ++at;

        return at;
    };

    if (key == "Backspace")
    {
        auto from = previous(position);
        value.erase(from, position - from);
        position = from;
    }
    else if (key == "Delete")
        value.erase(position, next(position) - position);
    else if (key == "Left")
        position = previous(position);
    else if (key == "Right")
        position = next(position);
    else if (key == "Home" || key == "C-a")
        position = 0;
    else if (key == "End" || key == "C-e")
        position = value.size();
    else if (key == "C-u")
    {
        value.erase(0, position);
        position = 0;
    }
    else if (key == "C-w")
    {
        auto from = position;

        while (from > 0 && value[from - 1] == ' ')
            --from;

        while (from > 0 && value[from - 1] != ' ')
            --from;

        value.erase(from, position - from);
        position = from;
    }
    else if (isCharacter(key))
    {
        value.insert(position, key);
        position += key.size();
    }
    else
        return false;

    return true;
}

void LineEdit::set(std::string text)
{
    value = std::move(text);
    position = value.size();
}

const std::string& LineEdit::text() const
{
    return value;
}

std::size_t LineEdit::cursor() const
{
    return position;
}

namespace
{
std::optional<FuzzyMatch> matchFrom(std::string_view query,
                                    std::string_view candidate,
                                    std::size_t start,
                                    bool ignoreCase)
{
    auto match = FuzzyMatch {};
    auto nameStart = candidate.rfind('/');
    nameStart = nameStart == std::string_view::npos ? 0 : nameStart + 1;

    auto q = std::size_t {0};
    auto previous = std::string_view::npos;

    for (auto i = start; i < candidate.size() && q < query.size(); ++i)
    {
        // Spaces in the query separate terms that may match anywhere in order.
        if (query[q] == ' ')
        {
            ++q;
            --i;
            continue;
        }

        if (fold(candidate[i], ignoreCase) != fold(query[q], ignoreCase))
            continue;

        auto score = 1;

        if (previous != std::string_view::npos && previous + 1 == i)
            score += 8;

        if (i == 0 || isBoundary(candidate[i - 1]))
            score += 6;

        if (i >= nameStart)
            score += 2;

        if (previous != std::string_view::npos)
            score -= static_cast<int>(std::min<std::size_t>(i - previous - 1, 5));

        match.score += score;
        match.positions.push_back(i);
        previous = i;
        ++q;
    }

    while (q < query.size() && query[q] == ' ')
        ++q;

    if (q < query.size())
        return std::nullopt;

    return match;
}
} // namespace

std::optional<FuzzyMatch> fuzzyMatch(std::string_view query,
                                     std::string_view candidate)
{
    while (!query.empty() && query.front() == ' ')
        query.remove_prefix(1);

    if (query.empty())
        return FuzzyMatch {};

    // Greedy from the first start settles for poor matches ("render" taking
    // the r of "Source/"), so try every start.
    auto ignoreCase = ignoresCase(query);
    auto best = std::optional<FuzzyMatch> {};

    for (std::size_t start = 0; start < candidate.size(); ++start)
    {
        if (fold(candidate[start], ignoreCase) != fold(query.front(), ignoreCase))
            continue;

        auto match = matchFrom(query, candidate, start, ignoreCase);

        if (!match)
            break; // a later start can't match what this one couldn't

        if (!best || match->score > best->score)
            best = std::move(match);
    }

    return best;
}

void Picker::setItems(std::vector<Item> items)
{
    all = std::move(items);
    refilter();
}

void Picker::setQuery(std::string query)
{
    text = std::move(query);
    row = 0;
    refilter();
}

void Picker::setFuzzy(bool fuzzyToUse)
{
    fuzzy = fuzzyToUse;
    refilter();
}

const std::string& Picker::query() const
{
    return text;
}

const std::vector<Item>& Picker::items() const
{
    return all;
}

const std::vector<int>& Picker::visible() const
{
    return shown;
}

std::vector<std::size_t> Picker::highlights(int visibleRow) const
{
    if (!fuzzy || visibleRow < 0 || visibleRow >= static_cast<int>(shown.size()))
        return {};

    auto match = fuzzyMatch(
        text,
        all[static_cast<std::size_t>(shown[static_cast<std::size_t>(visibleRow)])]
            .label);
    return match ? match->positions : std::vector<std::size_t> {};
}

int Picker::selected() const
{
    return shown.empty() ? -1
                         : std::clamp(row, 0, static_cast<int>(shown.size()) - 1);
}

void Picker::move(int delta)
{
    select(selected() + delta);
}

void Picker::select(int newRow)
{
    row = shown.empty() ? 0
                        : std::clamp(newRow, 0, static_cast<int>(shown.size()) - 1);
}

const Item* Picker::current() const
{
    auto at = selected();
    return at < 0
               ? nullptr
               : &all[static_cast<std::size_t>(shown[static_cast<std::size_t>(at)])];
}

void Picker::refilter()
{
    shown.clear();

    if (!fuzzy || text.empty())
    {
        for (std::size_t i = 0; i < all.size(); ++i)
            shown.push_back(static_cast<int>(i));

        return;
    }

    auto scored = std::vector<std::pair<int, int>> {};

    for (std::size_t i = 0; i < all.size(); ++i)
        if (auto match = fuzzyMatch(text, all[i].label))
            scored.push_back({match->score, static_cast<int>(i)});

    std::stable_sort(scored.begin(),
                     scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& [score, index]: scored)
        shown.push_back(index);
}
} // namespace telescope
