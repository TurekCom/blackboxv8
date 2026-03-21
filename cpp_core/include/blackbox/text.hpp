#pragma once

#include <string>
#include <string_view>

namespace blackbox {

enum class NumberMode {
    Cardinal,
    Digits,
};

std::wstring ToLowerPL(std::wstring_view text);
std::wstring ExpandNumbers(std::wstring_view text, NumberMode mode);
std::wstring NormalizePolishText(std::wstring_view text);

}  // namespace blackbox
