#pragma once

#include <string>
#include <type_traits>
#include <cstdio>

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

    if (length < 0) {
        abort();
    }

    std::string result(length, '\0');
    snprintf(result.data(), length + 1, fmt, detail::to_printf_arg(std::forward<Args>(args))...);

    return result;
}
