#include <TextBuffer.h>

#include <NanoTest/NanoTest.h>

#include <string>
#include <vector>

using namespace nano;
using namespace text;

auto tSplit = test("TextBuffer/splits text into lines") = []
{
    auto buffer = TextBuffer {"a\nb\n"};

    check(buffer.lineCount() == 3);
    check(buffer.line(2).empty());
    check(buffer.text() == "a\nb\n");
    check(TextBuffer {}.lineCount() == 1);
};

auto tInsert = test("TextBuffer/insert splits lines and returns the end") = []
{
    auto buffer = TextBuffer {"ad"};

    auto end = buffer.insert({0, 1}, "b\nc");
    check(buffer.text() == "ab\ncd");
    check(end == Position {1, 1});

    end = buffer.insert({0, 99}, "!");
    check(buffer.text() == "ab!\ncd");
    check(end == Position {0, 3});
};

auto tErase = test("TextBuffer/erase across lines returns what went") = []
{
    auto buffer = TextBuffer {"one\ntwo\nthree"};

    check(buffer.erase({0, 1}, {2, 2}) == "ne\ntwo\nth");
    check(buffer.text() == "oree");

    // Reversed ends are the same range.
    check(buffer.erase({0, 3}, {0, 1}) == "re");
    check(buffer.text() == "oe");
};

auto tLines = test("TextBuffer/insertLines eraseLines replaceLine") = []
{
    auto buffer = TextBuffer {"a\nd"};
    buffer.insertLines(1, {"b", "c"});
    check(buffer.text() == "a\nb\nc\nd");

    auto removed = buffer.eraseLines(1, 2);
    check(removed == std::vector<std::string> {"b", "c"});
    check(buffer.text() == "a\nd");

    buffer.replaceLine(1, "z");
    check(buffer.text() == "a\nz");

    buffer.eraseLines(0, 10);
    check(buffer.lineCount() == 1 && buffer.line(0).empty());
};

auto tUndoRedo = test("TextBuffer/undo and redo restore text and cursor") = []
{
    auto buffer = TextBuffer {"abc"};

    buffer.snapshot({0, 1});
    buffer.insert({0, 3}, "d");
    buffer.snapshot({0, 3});
    buffer.erase({0, 0}, {0, 1});
    check(buffer.text() == "bcd");

    auto cursor = buffer.undo({0, 0});
    check(cursor && *cursor == Position {0, 3});
    check(buffer.text() == "abcd");

    cursor = buffer.undo({0, 3});
    check(cursor && *cursor == Position {0, 1});
    check(buffer.text() == "abc");
    check(!buffer.undo({0, 0}));

    cursor = buffer.redo({0, 1});
    check(cursor && *cursor == Position {0, 3});
    check(buffer.text() == "abcd");
    check(buffer.canRedo());

    buffer.snapshot({0, 0}); // a new change drops the redo history
    check(!buffer.canRedo());
};

auto tSetTextClearsHistory = test("TextBuffer/setText forgets undo") = []
{
    auto buffer = TextBuffer {"a"};
    buffer.snapshot({});
    buffer.setText("b");
    check(!buffer.canUndo());
};

auto tSmartcase = test("TextBuffer/smartcase") = []
{
    check(ignoresCase("foo"));
    check(ignoresCase("foo_1 \xC3\x89")); // non-ASCII doesn't count as uppercase
    check(!ignoresCase("Foo"));
};

auto tFindAll = test("TextBuffer/findAll finds every match, non-overlapping") = []
{
    auto matches = findAll("Foo foo FOO", "foo");
    check(matches.size() == 3);
    check(matches[1].start == 4 && matches[1].length == 3);

    check(findAll("Foo foo FOO", "Foo").size() == 1);
    check(findAll("aaaa", "aa").size() == 2);
    check(findAll("abc", "").empty());
    check(findAll("ab", "abc").empty());
};

auto tFindAllUtf8 = test("TextBuffer/findAll matches UTF-8 exactly") = []
{
    auto line = std::string {"caf\xC3\xA9 CAF\xC3\xA9"};
    auto matches = findAll(line, "caf\xC3\xA9");

    check(matches.size() == 2);
    check(matches[1].start == 6 && matches[1].length == 5);
    check(findAll(line, "\xC3\xA9").size() == 2);
};

auto tSearchForward = test("TextBuffer/search forward, then wrapping") = []
{
    auto lines = std::vector<std::string> {"x foo", "bar", "foo foo"};

    auto hit = search(lines, "foo", {0, 2}, true);
    check(hit && hit->at == Position {2, 0} && !hit->wrapped);
    check(hit->length == 3);

    hit = search(lines, "foo", {2, 0}, true);
    check(hit && hit->at == Position {2, 4} && !hit->wrapped);

    hit = search(lines, "foo", {2, 4}, true);
    check(hit && hit->at == Position {0, 2} && hit->wrapped);
};

auto tSearchBackward = test("TextBuffer/search backward, then wrapping") = []
{
    auto lines = std::vector<std::string> {"x foo", "bar", "foo foo"};

    auto hit = search(lines, "foo", {2, 4}, false);
    check(hit && hit->at == Position {2, 0} && !hit->wrapped);

    hit = search(lines, "foo", {2, 0}, false);
    check(hit && hit->at == Position {0, 2} && !hit->wrapped);

    hit = search(lines, "foo", {0, 2}, false);
    check(hit && hit->at == Position {2, 4} && hit->wrapped);
};

auto tSearchOnlyMatch =
    test("TextBuffer/a lone match is found again by wrapping") = []
{
    auto lines = std::vector<std::string> {"one", "two"};

    auto hit = search(lines, "TWO", {1, 0}, true);
    check(!hit); // smartcase: uppercase needle is exact

    hit = search(lines, "two", {1, 0}, true);
    check(hit && hit->at == Position {1, 0} && hit->wrapped);

    hit = search(lines, "two", {1, 0}, false);
    check(hit && hit->at == Position {1, 0} && hit->wrapped);

    check(!search(lines, "zzz", {0, 0}, true));
    check(!search(std::vector<std::string> {}, "a", {}, true));
};
