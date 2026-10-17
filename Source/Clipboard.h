#pragma once

#include <optional>
#include <string>

namespace diffview::clipboard
{
// PNG bytes. macOS only; nullopt elsewhere.
std::optional<std::string> imagePng();
} // namespace diffview::clipboard
