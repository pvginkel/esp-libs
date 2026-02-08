#pragma once

#include <cstdio>
#include <string>
#include <type_traits>

#include "error.h"

namespace detail {

template <typename T>
auto to_printf_arg(T&& arg) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return arg.c_str();
    } else {
        return std::forward<T>(arg);
    }
}

}  // namespace detail

template <typename... Args>
std::string strformat(const char* fmt, Args&&... args) {
    auto length = snprintf(nullptr, 0, fmt, detail::to_printf_arg(std::forward<Args>(args))...);
    ESP_ASSERT_CHECK(length >= 0);

    std::string result(length, '\0');
    snprintf(result.data(), length + 1, fmt, detail::to_printf_arg(std::forward<Args>(args))...);

    return result;
}
