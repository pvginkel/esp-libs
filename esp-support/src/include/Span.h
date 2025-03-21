#pragma once

template <typename T>
class Span {
    T* _buffer;
    size_t _len;

public:
    Span(T* buffer, size_t len) : _buffer(buffer), _len(len) {}

    T* buffer() { return _buffer; }
    size_t len() { return _len; }
};
