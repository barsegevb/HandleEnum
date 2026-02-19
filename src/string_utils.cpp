#include "string_utils.hpp"

#include <windows.h>

#include <cctype>

namespace utils {

std::string to_lower_ascii(const std::string_view text) {
    std::string lower;
    lower.reserve(text.size());
    for (const char ch : text) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lower;
}

bool equals_ignore_case(const std::string_view left, const std::string_view right) {
    return to_lower_ascii(left) == to_lower_ascii(right);
}

bool contains_ignore_case(const std::string_view text, const std::string_view needle) {
    const std::string lower_text = to_lower_ascii(text);
    const std::string lower_needle = to_lower_ascii(needle);
    return lower_text.find(lower_needle) != std::string::npos;
}

std::string utf16_to_utf8(const std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }

    const int size = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(size), '\0');
    const int written = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        utf8.data(),
        size,
        nullptr,
        nullptr
    );

    if (written <= 0) {
        return {};
    }

    return utf8;
}

} // namespace utils
