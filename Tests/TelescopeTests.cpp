#include <Telescope.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace telescope;

namespace
{
std::vector<Item> files()
{
    return {{"Lib/DiffView/Diff.cpp", {}, "a"},
            {"Source/Render.cpp", {}, "b"},
            {"Tests/RenderTests.cpp", {}, "c"}};
}
} // namespace

auto tFuzzy = test("Telescope/fuzzy matches in order with gaps, smartcase") = []
{
    check(fuzzyMatch("vwc", "Source/Viewer.cpp").has_value());
    check(!fuzzyMatch("xyz", "Source/Viewer.cpp"));
    check(!fuzzyMatch("Vw", "source/viewer.cpp"));
    check(fuzzyMatch("src rend", "Source/Render.cpp").has_value());
};

auto tRanks = test("Telescope/file names and runs rank first") = []
{
    auto picker = Picker {};
    picker.setItems(files());
    picker.setQuery("render");

    check(picker.visible().size() == 2);
    check(picker.current()->key == "b");
    check(picker.highlights(0).size() == 6);
};

auto tEmptyQuery = test("Telescope/an empty query shows everything in order") = []
{
    auto picker = Picker {};
    picker.setItems(files());

    check(picker.visible().size() == 3);
    check(picker.current()->key == "a");
};

auto tSelection = test("Telescope/selection clamps and resets on a new query") = []
{
    auto picker = Picker {};
    picker.setItems(files());
    picker.move(5);
    check(picker.selected() == 2);
    picker.move(-10);
    check(picker.selected() == 0);
    picker.move(1);
    picker.setQuery("cpp");
    check(picker.selected() == 0);

    picker.setQuery("nothing matches");
    check(picker.selected() == -1);
    check(picker.current() == nullptr);
};

auto tLive = test("Telescope/with fuzzy off the host's items show as given") = []
{
    auto picker = Picker {};
    picker.setFuzzy(false);
    picker.setQuery("zzz");
    picker.setItems(files());

    check(picker.visible().size() == 3);
    check(picker.highlights(0).empty());
};

auto tLineEdit = test("Telescope/LineEdit edits one line") = []
{
    auto line = LineEdit {};

    check(line.feed("a"));
    check(line.feed("\xC3\xA9"));
    check(line.feed("b"));
    check(line.text()
          == "a\xC3\xA9"
             "b");
    check(line.feed("Left"));
    check(line.feed("Backspace"));
    check(line.text() == "ab" && line.cursor() == 1);
    check(!line.feed("Tab"));
    check(!line.feed("F1"));
    check(!line.feed("Enter"));

    line.set("one two three");
    check(line.feed("C-w"));
    check(line.text() == "one two ");
    check(line.feed("C-u"));
    check(line.text().empty());
};
