# DiffView

`dv` (`~/.local/bin`) is in daily use by Jamie and every agent on this
machine. Keep it current: after any change that builds, run `just test`
then `just install`. Never leave the installed binary behind the tree or
broken.

## Code style

Follow eacp's style guide (the "Code Style" section of
`build/_deps/eacp-src/CLAUDE.md`, or `~/projects/eacp/CLAUDE.md`) and run
clang-format on what you touch. No comments unless the code can't say it.
Functions are named with verbs. No defaulted `operator==` unless something
needs it.

- No nested or multi-line ternaries: an immediately invoked lambda with
  plain ifs, or a small named function.
- No nullable raw pointers: `std::optional`, an index, or a reference.
- Braces on any loop or `if` whose body is another loop or `if`, or spans
  lines; flatten with early `continue`/`return` where it reads better.
- File IO through eacp's `Files` (`readFile`, `listFiles`, `exists`,
  `appendFile`, ...), not hand-rolled streams; the review store's are in
  `Source/Store.h`.
