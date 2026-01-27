#pragma once

#include <utility>

template <typename F>
class Defer final {
    F func_;
    bool active_;

public:
    explicit Defer(F&& f) : func_(std::forward<F>(f)), active_(true) {}

    ~Defer() noexcept {
        if (active_) {
            func_();
        }
    }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;

    Defer(Defer&& other) noexcept : func_(std::move(other.func_)), active_(other.active_) { other.active_ = false; }
};

#ifndef CONCAT
#ifndef CONCAT_IMPL
#define CONCAT_IMPL(x, y) x##y
#endif  // CONCAT_IMPL
#define CONCAT(x, y) CONCAT_IMPL(x, y)
#endif  // CONCAT

#define DEFER(code) Defer CONCAT(_defer_, __LINE__)([&] { code; })
