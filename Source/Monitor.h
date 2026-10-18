#pragma once

#include <string>

namespace diffview
{
// `dv monitor`: no terminal, no UI. Prints a line for each review round
// submitted after it starts, until killed (or the first, with `once`).
int runMonitor(const std::string& repo, bool once);
} // namespace diffview
