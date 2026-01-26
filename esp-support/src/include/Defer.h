#pragma once

#include <utility>

template <typename F>
class Defer final {
    F func_;
    bool active_;

public:
    explicit Defer(F&& f) : func_(std::forward<F>(f)), active_(true) {}

    ~Defer() {
        if (active_) {
            func_();
        }
    }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;

    Defer(Defer&& other) noexcept : func_(std::move(other.func_)), active_(other.active_) { other.active_ = false; }
};

template <typename F>
Defer<F> defer(F&& f) {
    return Defer<F>(std::forward<F>(f));
}

#ifndef CONCAT
#ifndef CONCAT_IMPL
#define CONCAT_IMPL(x, y) x##y
#endif  // CONCAT_IMPL
#define CONCAT(x, y) CONCAT_IMPL(x, y)
#endif  // CONCAT

#define DEFER(code) auto CONCAT(_defer_, __LINE__) = defer([&] { code; })