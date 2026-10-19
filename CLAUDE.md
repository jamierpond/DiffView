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
