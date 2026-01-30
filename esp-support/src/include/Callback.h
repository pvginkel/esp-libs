#pragma once

#include <atomic>
#include <functional>

#include "Queue.h"

template <typename Arg>
class Callback {
    struct Node {
        std::function<void(Arg)> func;
        Node* next;

        Node(std::function<void(Arg)> func, Node* next) : func(std::move(func)), next(next) {}
    };

    std::atomic<Node*> _head{nullptr};

public:
    // Not thread-safe vs concurrent add/call.
    ~Callback() {
        auto n = _head.load(std::memory_order_relaxed);
        while (n) {
            auto next = n->next;
            delete n;
            n = next;
        }
    }

    void add(std::function<void(Arg)> func) {
        auto node = new Node(std::move(func), nullptr);

        // Treiber push: publish n as new head.
        Node* old = _head.load(std::memory_order_relaxed);
        do {
            node->next = old;  // only this thread writes n->next, before publish
        } while (!_head.compare_exchange_weak(old, node,
                                              std::memory_order_release,  // publish node contents (func + next)
                                              std::memory_order_relaxed   // on failure, just retry
                                              ));
    }

    bool call(Arg arg) const {
        // Snapshot head. Acquire pairs with add()'s release so the constructed
        // node (func/next) is visible to this thread.

        auto node = _head.load(std::memory_order_acquire);

        if (!node) {
            return false;
        }

        while (node) {
            node->func(arg);
            node = node->next;
        }

        return true;
    }

    void queue(Queue* queue, Arg arg) {
        queue->enqueue([this, arg] { call(arg); });
    }
};

template <>
class Callback<void> {
    struct Node {
        std::function<void()> func;
        Node* next;

        Node(std::function<void()> func, Node* next) : func(std::move(func)), next(next) {}
    };

    std::atomic<Node*> _head{nullptr};

public:
    // Not thread-safe vs concurrent add/call.
    ~Callback() {
        auto n = _head.load(std::memory_order_relaxed);
        while (n) {
            auto next = n->next;
            delete n;
            n = next;
        }
    }

    void add(std::function<void()> func) {
        auto node = new Node(std::move(func), nullptr);

        // Treiber push: publish n as new head.
        Node* old = _head.load(std::memory_order_relaxed);
        do {
            node->next = old;  // only this thread writes n->next, before publish
        } while (!_head.compare_exchange_weak(old, node,
                                              std::memory_order_release,  // publish node contents (func + next)
                                              std::memory_order_relaxed   // on failure, just retry
                                              ));
    }

    bool call() const {
        // Snapshot head. Acquire pairs with add()'s release so the constructed
        // node (func/next) is visible to this thread.

        auto node = _head.load(std::memory_order_acquire);

        if (!node) {
            return false;
        }

        while (node) {
            node->func();
            node = node->next;
        }

        return true;
    }

    void queue(Queue* queue) {
        queue->enqueue([this] { call(); });
    }
};
