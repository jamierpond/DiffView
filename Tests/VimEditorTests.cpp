#include <VimEditor.h>

#include <NanoTest/NanoTest.h>

#include <initializer_list>
#include <string>

using namespace nano;
using namespace vim;

namespace
{
void type(Editor& editor, std::initializer_list<std::string> keys)
{
    for (const auto& key: keys)
        editor.feed(key);
}

// Each character of `text` as its own key.
void typeText(Editor& editor, const std::string& text)
{
    for (auto c: text)
        editor.feed(std::string(1, c));
}
} // namespace

auto tInsertEscape = test("VimEditor/insert then escape") = []
{
    auto editor = Editor {};
    check(editor.mode() == Mode::normal);

    editor.feed("i");
    check(editor.mode() == Mode::insert);
    typeText(editor, "hello");
    editor.feed("Enter");
    typeText(editor, "world");
    check(editor.text() == "hello\nworld");

    editor.feed("Esc");
    check(editor.mode() == Mode::normal);
    check(editor.row() == 1 && editor.column() == 4);
};

auto tAppendOpen = test("VimEditor/a A I o O position the cursor") = []
{
    auto editor = Editor {"  abc"};
    type(editor, {"A", "!", "Esc"});
    check(editor.text() == "  abc!");

    type(editor, {"I", "#", "Esc"});
    check(editor.text() == "  #abc!");

    type(editor, {"o", "x", "Esc", "O", "y", "Esc"});
    check(editor.text() == "  #abc!\ny\nx");
};

auto tBackspaceJoins = test("VimEditor/backspace at line start joins lines") = []
{
    auto editor = Editor {"ab\ncd"};
    type(editor, {"j", "i", "Backspace"});
    check(editor.text() == "abcd");
    check(editor.row() == 0 && editor.column() == 2);
};

auto tWordMotions = test("VimEditor/w b e move by words") = []
{
    auto editor = Editor {"foo bar.baz\nqux"};

    editor.feed("w");
    check(editor.column() == 4);
    editor.feed("w");
    check(editor.column() == 7); // the '.'
    editor.feed("w");
    check(editor.column() == 8);
    editor.feed("w");
    check(editor.row() == 1 && editor.column() == 0);

    editor.feed("b");
    check(editor.row() == 0 && editor.column() == 8);

    type(editor, {"0", "e"});
    check(editor.column() == 2);
    editor.feed("e");
    check(editor.column() == 6);
};

auto tLineMotions = test("VimEditor/0 ^ $ gg G") = []
{
    auto editor = Editor {"  one\ntwo\nthree"};
    editor.feed("$");
    check(editor.column() == 4);
    editor.feed("0");
    check(editor.column() == 0);
    editor.feed("^");
    check(editor.column() == 2);
    editor.feed("G");
    check(editor.row() == 2);
    type(editor, {"g", "g"});
    check(editor.row() == 0);
    type(editor, {"2", "G"});
    check(editor.row() == 1);
};

auto tDeletePut = test("VimEditor/dd then p puts the line below") = []
{
    auto editor = Editor {"one\ntwo\nthree"};
    type(editor, {"d", "d"});
    check(editor.text() == "two\nthree");

    editor.feed("p");
    check(editor.text() == "two\none\nthree");
    check(editor.row() == 1);

    type(editor, {"y", "y", "P"});
    check(editor.text() == "two\none\none\nthree");
};

auto tCharwisePut = test("VimEditor/x then p puts the character after") = []
{
    auto editor = Editor {"abc"};
    editor.feed("x");
    check(editor.text() == "bc");
    editor.feed("p");
    check(editor.text() == "bac");
};

auto tOperators = test("VimEditor/dw cw ciw") = []
{
    auto editor = Editor {"one two three"};
    type(editor, {"d", "w"});
    check(editor.text() == "two three");

    type(editor, {"c", "w"});
    check(editor.mode() == Mode::insert);
    typeText(editor, "2");
    editor.feed("Esc");
    check(editor.text() == "2 three");

    type(editor, {"w", "l", "c", "i", "w"});
    typeText(editor, "3");
    editor.feed("Esc");
    check(editor.text() == "2 3");

    type(editor, {"0", "d", "i", "w"});
    check(editor.text() == " 3");
};

auto tLineOperators = test("VimEditor/D C cc J") = []
{
    auto editor = Editor {"abc def\nghi"};
    type(editor, {"w", "D"});
    check(editor.text() == "abc \nghi");

    type(editor, {"0", "J"});
    check(editor.text() == "abc ghi");

    type(editor, {"c", "c"});
    typeText(editor, "new");
    editor.feed("Esc");
    check(editor.text() == "new");
};

auto tCounts = test("VimEditor/counts repeat") = []
{
    auto editor = Editor {"abcdef\n1\n2\n3\n4"};
    type(editor, {"3", "x"});
    check(editor.lines()[0] == "def");

    type(editor, {"2", "j"});
    check(editor.row() == 2);

    type(editor, {"2", "d", "d"});
    check(editor.text() == "def\n1\n4");
};

auto tUndoRedo = test("VimEditor/u undoes a whole insert, C-r redoes") = []
{
    auto editor = Editor {"x"};
    type(editor, {"A"});
    typeText(editor, "yz");
    editor.feed("Esc");
    check(editor.text() == "xyz");

    editor.feed("u");
    check(editor.text() == "x");

    editor.feed("C-r");
    check(editor.text() == "xyz");

    type(editor, {"d", "d", "u"});
    check(editor.text() == "xyz");
};

auto tReplace = test("VimEditor/r replaces one character") = []
{
    auto editor = Editor {"cat"};
    type(editor, {"r", "b"});
    check(editor.text() == "bat");
};

auto tWrite = test("VimEditor/:w saves and stays") = []
{
    auto editor = Editor {"a"};
    type(editor, {"A", "b", "Esc"});
    check(editor.modified());

    type(editor, {":", "w"});
    check(editor.mode() == Mode::command);
    check(editor.commandLine() == "w");
    check(editor.feed("Enter") == Outcome::save);
    check(editor.mode() == Mode::normal);
    check(!editor.modified());
};

auto tWriteQuit = test("VimEditor/:wq and :x save and close") = []
{
    auto editor = Editor {"a"};
    type(editor, {":", "w", "q"});
    check(editor.feed("Enter") == Outcome::saveAndClose);

    type(editor, {":", "x"});
    check(editor.feed("Enter") == Outcome::saveAndClose);
};

auto tQuit = test("VimEditor/:q refuses unsaved changes, :q! discards") = []
{
    auto editor = Editor {"a"};
    type(editor, {":", "q"});
    check(editor.feed("Enter") == Outcome::close);

    type(editor, {"x", ":", "q"});
    check(editor.feed("Enter") == Outcome::none);
    check(editor.message().starts_with("E37"));

    type(editor, {":", "q", "!"});
    check(editor.feed("Enter") == Outcome::discard);
};

auto tBadCommand = test("VimEditor/an unknown command says so") = []
{
    auto editor = Editor {};
    type(editor, {":", "z", "z"});
    check(editor.feed("Enter") == Outcome::none);
    check(editor.message() == "E492: Not an editor command: zz");

    editor.feed("j");
    check(editor.message().empty());
};

auto tCommandBackspace =
    test("VimEditor/backspace on an empty command line leaves it") = []
{
    auto editor = Editor {};
    type(editor, {":", "w", "Backspace"});
    check(editor.commandLine().empty());
    check(editor.mode() == Mode::command);
    editor.feed("Backspace");
    check(editor.mode() == Mode::normal);
};

auto tControlKeys = test("VimEditor/C-s saves and closes, C-v asks to paste") = []
{
    auto editor = Editor {};
    editor.feed("i");
    typeText(editor, "hi");
    check(editor.feed("C-v") == Outcome::paste);
    check(editor.mode() == Mode::insert);
    check(editor.feed("C-s") == Outcome::saveAndClose);
    check(!editor.modified());
};

auto tInsertText =
    test("VimEditor/insertText splits lines and is one undo step") = []
{
    auto editor = Editor {"ab"};
    type(editor, {"l", "i"});
    editor.insertText("1\n2\n3");
    check(editor.text() == "a1\n2\n3b");
    check(editor.row() == 2 && editor.column() == 1);
    check(editor.mode() == Mode::insert);

    editor.feed("Esc");
    editor.feed("u");
    check(editor.text() == "ab");
};

auto tWrapRows = test("VimEditor/soft wrap breaks after the last space") = []
{
    auto editor = Editor {"the quick brown fox\nab"};
    editor.setWrapWidth(10);
    auto rows = editor.visualRows();

    check(rows.size() == 3);
    check(rows[0].line == 0 && rows[0].start == 0 && rows[0].end == 10);
    check(rows[1].line == 0 && rows[1].start == 10 && rows[1].end == 19);
    check(rows[2].line == 1);

    auto hard = Editor {"abcdefghij"};
    hard.setWrapWidth(4);
    check(hard.visualRows().size() == 3);
};

auto tWrapMovement = test("VimEditor/j and k move by displayed row") = []
{
    auto editor = Editor {"the quick brown fox\nab"};
    editor.setWrapWidth(10);
    type(editor, {"l", "l"});

    editor.feed("j");
    check(editor.row() == 0 && editor.column() == 12);
    check(editor.cursorVisualRow() == 1);
    check(editor.cursorVisualColumn() == 2);

    editor.feed("j");
    check(editor.row() == 1 && editor.column() == 1);

    type(editor, {"k", "k"});
    check(editor.row() == 0 && editor.column() == 2);
};

auto tWrapEndOfRow =
    test("VimEditor/a cursor at the end of a full row stays on it") = []
{
    auto editor = Editor {"abcd"};
    editor.setWrapWidth(4);
    editor.feed("A");
    check(editor.column() == 4);
    check(editor.cursorVisualRow() == 0);
    check(editor.cursorVisualColumn() == 4);
};

auto tUtf8 = test("VimEditor/cursor moves by code point") = []
{
    auto editor = Editor {"\xC3\xA9t\xC3\xA9"}; // été
    editor.feed("l");
    check(editor.column() == 2);
    editor.feed("l");
    check(editor.column() == 3);
    editor.feed("l");
    check(editor.column() == 3); // last char, normal mode stops here

    editor.feed("x");
    check(editor.text() == "\xC3\xA9t");

    type(editor, {"A", "\xC3\xBC", "Esc"});
    check(editor.text() == "\xC3\xA9t\xC3\xBC");
    check(editor.column() == 3);
};

auto tInsertDeleteWord =
    test("VimEditor/C-w deletes the word behind the cursor") = []
{
    auto editor = Editor {};
    editor.feed("i");
    typeText(editor, "foo bar.baz  ");

    editor.feed("C-w");
    check(editor.text() == "foo bar.");
    editor.feed("C-w");
    check(editor.text() == "foo bar");
    editor.feed("C-w");
    check(editor.text() == "foo ");
    check(editor.mode() == Mode::insert);
};

auto tInsertDeleteLine = test("VimEditor/C-u deletes back to the line start") = []
{
    auto editor = Editor {"keep\nab cd"};
    type(editor, {"j", "A", "C-u"});
    check(editor.text() == "keep\n");
    check(editor.column() == 0);
};

auto tInsertJoinAtStart = test("VimEditor/C-w C-u C-h at column 0 join lines") = []
{
    for (const auto* key: {"C-w", "C-u", "C-h"})
    {
        auto editor = Editor {"ab\ncd"};
        type(editor, {"j", "i", key});
        check(editor.text() == "abcd");
        check(editor.row() == 0 && editor.column() == 2);
    }

    auto editor = Editor {"abc"};
    type(editor, {"A", "C-h"});
    check(editor.text() == "ab");
};

auto tIncsearch = test("VimEditor/search moves live and Esc goes back") = []
{
    auto editor = Editor {"alpha\nbeta\nalphabet"};
    editor.feed("/");
    check(editor.mode() == Mode::search);
    check(editor.promptChar() == '/');

    typeText(editor, "phab");
    check(editor.commandLine() == "phab");
    check(editor.row() == 2 && editor.column() == 2);

    auto match = editor.searchMatch();
    check(match && match->row == 2 && match->column == 2 && match->length == 4);

    editor.feed("Backspace"); // "pha": the first alpha, after the start
    check(editor.row() == 0 && editor.column() == 2);

    editor.feed("Esc");
    check(editor.mode() == Mode::normal);
    check(editor.row() == 0 && editor.column() == 0);
    check(!editor.searchMatch());
    check(editor.promptChar() == 0);
};

auto tSearchEnter = test("VimEditor/Enter keeps the match; n and N repeat") = []
{
    auto editor = Editor {"foo\nbar foo\nfoo"};
    editor.feed("/");
    typeText(editor, "foo");
    editor.feed("Enter");
    check(editor.mode() == Mode::normal);
    check(editor.row() == 1 && editor.column() == 4);

    editor.feed("n");
    check(editor.row() == 2 && editor.column() == 0);

    editor.feed("n");
    check(editor.row() == 0 && editor.column() == 0);
    check(editor.message() == "search hit BOTTOM, continuing at TOP");

    editor.feed("N");
    check(editor.row() == 2);
    check(editor.message() == "search hit TOP, continuing at BOTTOM");
};

auto tSearchBackward =
    test("VimEditor/? searches backward; empty Enter repeats") = []
{
    auto editor = Editor {"x1\nx2\nx3"};
    editor.feed("G");
    editor.feed("?");
    check(editor.promptChar() == '?');
    editor.feed("x");
    editor.feed("Enter");
    check(editor.row() == 1);

    type(editor, {"?", "Enter"});
    check(editor.row() == 0);

    editor.feed("n"); // same direction: backward
    check(editor.row() == 2);
};

auto tSearchSmartcase = test("VimEditor/search is smartcase") = []
{
    auto editor = Editor {"start\nFoo\nfoo"};
    type(editor, {"/", "f", "o", "o", "Enter"});
    check(editor.row() == 1);

    type(editor, {"g", "g", "/", "F", "o", "o", "Enter"});
    check(editor.row() == 1);
    editor.feed("n");
    check(editor.row() == 1); // the only match, found by wrapping
};

auto tSearchNotFound = test("VimEditor/a missing pattern says so") = []
{
    auto editor = Editor {"abc"};
    editor.feed("n");
    check(editor.message().starts_with("E35"));

    type(editor, {"/", "z", "Enter"});
    check(editor.message() == "E486: Pattern not found: z");
    check(editor.row() == 0 && editor.column() == 0);
};
