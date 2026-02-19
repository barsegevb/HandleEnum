#pragma once

#include <string>
#include <string_view>

namespace utils {

[[nodiscard]] std::string to_lower_ascii(std::string_view text);
[[nodiscard]] bool equals_ignore_case(std::string_view left, std::string_view right);
[[nodiscard]] bool contains_ignore_case(std::string_view text, std::string_view needle);
[[nodiscard]] std::string utf16_to_utf8(std::wstring_view wide);

} // namespace utils
