#include "support.h"

#include "ZigBeeStream.h"

LOG_TAG(ZigBeeStream);

// GENERATION START

int16_t ZigBeeStream::readInt16Le() {
    const int16_t result = ((int16_t)_data[_position + 0] << 0) | ((int16_t)_data[_position + 1] << 8);
    _position += 2;
    return result;
}

int16_t ZigBeeStream::readInt16Be() {
    const int16_t result = ((int16_t)_data[_position + 0] << 8) | ((int16_t)_data[_position + 1] << 0);
    _position += 2;
    return result;
}

int32_t ZigBeeStream::readInt24Le() {
    const int32_t result = ((int32_t)_data[_position + 0] << 0) | ((int32_t)_data[_position + 1] << 8) |
                           ((int32_t)_data[_position + 2] << 16);
    _position += 3;
    return result;
}

int32_t ZigBeeStream::readInt24Be() {
    const int32_t result = ((int32_t)_data[_position + 0] << 16) | ((int32_t)_data[_position + 1] << 8) |
                           ((int32_t)_data[_position + 2] << 0);
    _position += 3;
    return result;
}

int32_t ZigBeeStream::readInt32Le() {
    const int32_t result = ((int32_t)_data[_position + 0] << 0) | ((int32_t)_data[_position + 1] << 8) |
                           ((int32_t)_data[_position + 2] << 16) | ((int32_t)_data[_position + 3] << 24);
    _position += 4;
    return result;
}

int32_t ZigBeeStream::readInt32Be() {
    const int32_t result = ((int32_t)_data[_position + 0] << 24) | ((int32_t)_data[_position + 1] << 16) |
                           ((int32_t)_data[_position + 2] << 8) | ((int32_t)_data[_position + 3] << 0);
    _position += 4;
    return result;
}

int64_t ZigBeeStream::readInt40Le() {
    const int64_t result = ((int64_t)_data[_position + 0] << 0) | ((int64_t)_data[_position + 1] << 8) |
                           ((int64_t)_data[_position + 2] << 16) | ((int64_t)_data[_position + 3] << 24) |
                           ((int64_t)_data[_position + 4] << 32);
    _position += 5;
    return result;
}

int64_t ZigBeeStream::readInt40Be() {
    const int64_t result = ((int64_t)_data[_position + 0] << 32) | ((int64_t)_data[_position + 1] << 24) |
                           ((int64_t)_data[_position + 2] << 16) | ((int64_t)_data[_position + 3] << 8) |
                           ((int64_t)_data[_position + 4] << 0);
    _position += 5;
    return result;
}

int64_t ZigBeeStream::readInt48Le() {
    const int64_t result = ((int64_t)_data[_position + 0] << 0) | ((int64_t)_data[_position + 1] << 8) |
                           ((int64_t)_data[_position + 2] << 16) | ((int64_t)_data[_position + 3] << 24) |
                           ((int64_t)_data[_position + 4] << 32) | ((int64_t)_data[_position + 5] << 40);
    _position += 6;
    return result;
}

int64_t ZigBeeStream::readInt48Be() {
    const int64_t result = ((int64_t)_data[_position + 0] << 40) | ((int64_t)_data[_position + 1] << 32) |
                           ((int64_t)_data[_position + 2] << 24) | ((int64_t)_data[_position + 3] << 16) |
                           ((int64_t)_data[_position + 4] << 8) | ((int64_t)_data[_position + 5] << 0);
    _position += 6;
    return result;
}

int64_t ZigBeeStream::readInt56Le() {
    const int64_t result = ((int64_t)_data[_position + 0] << 0) | ((int64_t)_data[_position + 1] << 8) |
                           ((int64_t)_data[_position + 2] << 16) | ((int64_t)_data[_position + 3] << 24) |
                           ((int64_t)_data[_position + 4] << 32) | ((int64_t)_data[_position + 5] << 40) |
                           ((int64_t)_data[_position + 6] << 48);
    _position += 7;
    return result;
}

int64_t ZigBeeStream::readInt56Be() {
    const int64_t result = ((int64_t)_data[_position + 0] << 48) | ((int64_t)_data[_position + 1] << 40) |
                           ((int64_t)_data[_position + 2] << 32) | ((int64_t)_data[_position + 3] << 24) |
                           ((int64_t)_data[_position + 4] << 16) | ((int64_t)_data[_position + 5] << 8) |
                           ((int64_t)_data[_position + 6] << 0);
    _position += 7;
    return result;
}

int64_t ZigBeeStream::readInt64Le() {
    const int64_t result = ((int64_t)_data[_position + 0] << 0) | ((int64_t)_data[_position + 1] << 8) |
                           ((int64_t)_data[_position + 2] << 16) | ((int64_t)_data[_position + 3] << 24) |
                           ((int64_t)_data[_position + 4] << 32) | ((int64_t)_data[_position + 5] << 40) |
                           ((int64_t)_data[_position + 6] << 48) | ((int64_t)_data[_position + 7] << 56);
    _position += 8;
    return result;
}

int64_t ZigBeeStream::readInt64Be() {
    const int64_t result = ((int64_t)_data[_position + 0] << 56) | ((int64_t)_data[_position + 1] << 48) |
                           ((int64_t)_data[_position + 2] << 40) | ((int64_t)_data[_position + 3] << 32) |
                           ((int64_t)_data[_position + 4] << 24) | ((int64_t)_data[_position + 5] << 16) |
                           ((int64_t)_data[_position + 6] << 8) | ((int64_t)_data[_position + 7] << 0);
    _position += 8;
    return result;
}

uint16_t ZigBeeStream::readUInt16Le() {
    const uint16_t result = ((uint16_t)_data[_position + 0] << 0) | ((uint16_t)_data[_position + 1] << 8);
    _position += 2;
    return result;
}

uint16_t ZigBeeStream::readUInt16Be() {
    const uint16_t result = ((uint16_t)_data[_position + 0] << 8) | ((uint16_t)_data[_position + 1] << 0);
    _position += 2;
    return result;
}

uint32_t ZigBeeStream::readUInt24Le() {
    const uint32_t result = ((uint32_t)_data[_position + 0] << 0) | ((uint32_t)_data[_position + 1] << 8) |
                            ((uint32_t)_data[_position + 2] << 16);
    _position += 3;
    return result;
}

uint32_t ZigBeeStream::readUInt24Be() {
    const uint32_t result = ((uint32_t)_data[_position + 0] << 16) | ((uint32_t)_data[_position + 1] << 8) |
                            ((uint32_t)_data[_position + 2] << 0);
    _position += 3;
    return result;
}

uint32_t ZigBeeStream::readUInt32Le() {
    const uint32_t result = ((uint32_t)_data[_position + 0] << 0) | ((uint32_t)_data[_position + 1] << 8) |
                            ((uint32_t)_data[_position + 2] << 16) | ((uint32_t)_data[_position + 3] << 24);
    _position += 4;
    return result;
}

uint32_t ZigBeeStream::readUInt32Be() {
    const uint32_t result = ((uint32_t)_data[_position + 0] << 24) | ((uint32_t)_data[_position + 1] << 16) |
                            ((uint32_t)_data[_position + 2] << 8) | ((uint32_t)_data[_position + 3] << 0);
    _position += 4;
    return result;
}

uint64_t ZigBeeStream::readUInt40Le() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 0) | ((uint64_t)_data[_position + 1] << 8) |
                            ((uint64_t)_data[_position + 2] << 16) | ((uint64_t)_data[_position + 3] << 24) |
                            ((uint64_t)_data[_position + 4] << 32);
    _position += 5;
    return result;
}

uint64_t ZigBeeStream::readUInt40Be() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 32) | ((uint64_t)_data[_position + 1] << 24) |
                            ((uint64_t)_data[_position + 2] << 16) | ((uint64_t)_data[_position + 3] << 8) |
                            ((uint64_t)_data[_position + 4] << 0);
    _position += 5;
    return result;
}

uint64_t ZigBeeStream::readUInt48Le() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 0) | ((uint64_t)_data[_position + 1] << 8) |
                            ((uint64_t)_data[_position + 2] << 16) | ((uint64_t)_data[_position + 3] << 24) |
                            ((uint64_t)_data[_position + 4] << 32) | ((uint64_t)_data[_position + 5] << 40);
    _position += 6;
    return result;
}

uint64_t ZigBeeStream::readUInt48Be() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 40) | ((uint64_t)_data[_position + 1] << 32) |
                            ((uint64_t)_data[_position + 2] << 24) | ((uint64_t)_data[_position + 3] << 16) |
                            ((uint64_t)_data[_position + 4] << 8) | ((uint64_t)_data[_position + 5] << 0);
    _position += 6;
    return result;
}

uint64_t ZigBeeStream::readUInt56Le() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 0) | ((uint64_t)_data[_position + 1] << 8) |
                            ((uint64_t)_data[_position + 2] << 16) | ((uint64_t)_data[_position + 3] << 24) |
                            ((uint64_t)_data[_position + 4] << 32) | ((uint64_t)_data[_position + 5] << 40) |
                            ((uint64_t)_data[_position + 6] << 48);
    _position += 7;
    return result;
}

uint64_t ZigBeeStream::readUInt56Be() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 48) | ((uint64_t)_data[_position + 1] << 40) |
                            ((uint64_t)_data[_position + 2] << 32) | ((uint64_t)_data[_position + 3] << 24) |
                            ((uint64_t)_data[_position + 4] << 16) | ((uint64_t)_data[_position + 5] << 8) |
                            ((uint64_t)_data[_position + 6] << 0);
    _position += 7;
    return result;
}

uint64_t ZigBeeStream::readUInt64Le() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 0) | ((uint64_t)_data[_position + 1] << 8) |
                            ((uint64_t)_data[_position + 2] << 16) | ((uint64_t)_data[_position + 3] << 24) |
                            ((uint64_t)_data[_position + 4] << 32) | ((uint64_t)_data[_position + 5] << 40) |
                            ((uint64_t)_data[_position + 6] << 48) | ((uint64_t)_data[_position + 7] << 56);
    _position += 8;
    return result;
}

uint64_t ZigBeeStream::readUInt64Be() {
    const uint64_t result = ((uint64_t)_data[_position + 0] << 56) | ((uint64_t)_data[_position + 1] << 48) |
                            ((uint64_t)_data[_position + 2] << 40) | ((uint64_t)_data[_position + 3] << 32) |
                            ((uint64_t)_data[_position + 4] << 24) | ((uint64_t)_data[_position + 5] << 16) |
                            ((uint64_t)_data[_position + 6] << 8) | ((uint64_t)_data[_position + 7] << 0);
    _position += 8;
    return result;
}

void ZigBeeStream::writeInt16Le(int16_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _position += 2;
}

void ZigBeeStream::writeInt16Be(int16_t value) {
    _data[_position + 0] = (value >> 8) & 0xff;
    _data[_position + 1] = (value >> 0) & 0xff;
    _position += 2;
}

void ZigBeeStream::writeInt24Le(int32_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _position += 3;
}

void ZigBeeStream::writeInt24Be(int32_t value) {
    _data[_position + 0] = (value >> 16) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 0) & 0xff;
    _position += 3;
}

void ZigBeeStream::writeInt32Le(int32_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _position += 4;
}

void ZigBeeStream::writeInt32Be(int32_t value) {
    _data[_position + 0] = (value >> 24) & 0xff;
    _data[_position + 1] = (value >> 16) & 0xff;
    _data[_position + 2] = (value >> 8) & 0xff;
    _data[_position + 3] = (value >> 0) & 0xff;
    _position += 4;
}

void ZigBeeStream::writeInt40Le(int64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _position += 5;
}

void ZigBeeStream::writeInt40Be(int64_t value) {
    _data[_position + 0] = (value >> 32) & 0xff;
    _data[_position + 1] = (value >> 24) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 8) & 0xff;
    _data[_position + 4] = (value >> 0) & 0xff;
    _position += 5;
}

void ZigBeeStream::writeInt48Le(int64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _position += 6;
}

void ZigBeeStream::writeInt48Be(int64_t value) {
    _data[_position + 0] = (value >> 40) & 0xff;
    _data[_position + 1] = (value >> 32) & 0xff;
    _data[_position + 2] = (value >> 24) & 0xff;
    _data[_position + 3] = (value >> 16) & 0xff;
    _data[_position + 4] = (value >> 8) & 0xff;
    _data[_position + 5] = (value >> 0) & 0xff;
    _position += 6;
}

void ZigBeeStream::writeInt56Le(int64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _data[_position + 6] = (value >> 48) & 0xff;
    _position += 7;
}

void ZigBeeStream::writeInt56Be(int64_t value) {
    _data[_position + 0] = (value >> 48) & 0xff;
    _data[_position + 1] = (value >> 40) & 0xff;
    _data[_position + 2] = (value >> 32) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 16) & 0xff;
    _data[_position + 5] = (value >> 8) & 0xff;
    _data[_position + 6] = (value >> 0) & 0xff;
    _position += 7;
}

void ZigBeeStream::writeInt64Le(int64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _data[_position + 6] = (value >> 48) & 0xff;
    _data[_position + 7] = (value >> 56) & 0xff;
    _position += 8;
}

void ZigBeeStream::writeInt64Be(int64_t value) {
    _data[_position + 0] = (value >> 56) & 0xff;
    _data[_position + 1] = (value >> 48) & 0xff;
    _data[_position + 2] = (value >> 40) & 0xff;
    _data[_position + 3] = (value >> 32) & 0xff;
    _data[_position + 4] = (value >> 24) & 0xff;
    _data[_position + 5] = (value >> 16) & 0xff;
    _data[_position + 6] = (value >> 8) & 0xff;
    _data[_position + 7] = (value >> 0) & 0xff;
    _position += 8;
}

void ZigBeeStream::writeUInt16Le(uint16_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _position += 2;
}

void ZigBeeStream::writeUInt16Be(uint16_t value) {
    _data[_position + 0] = (value >> 8) & 0xff;
    _data[_position + 1] = (value >> 0) & 0xff;
    _position += 2;
}

void ZigBeeStream::writeUInt24Le(uint32_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _position += 3;
}

void ZigBeeStream::writeUInt24Be(uint32_t value) {
    _data[_position + 0] = (value >> 16) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 0) & 0xff;
    _position += 3;
}

void ZigBeeStream::writeUInt32Le(uint32_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _position += 4;
}

void ZigBeeStream::writeUInt32Be(uint32_t value) {
    _data[_position + 0] = (value >> 24) & 0xff;
    _data[_position + 1] = (value >> 16) & 0xff;
    _data[_position + 2] = (value >> 8) & 0xff;
    _data[_position + 3] = (value >> 0) & 0xff;
    _position += 4;
}

void ZigBeeStream::writeUInt40Le(uint64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _position += 5;
}

void ZigBeeStream::writeUInt40Be(uint64_t value) {
    _data[_position + 0] = (value >> 32) & 0xff;
    _data[_position + 1] = (value >> 24) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 8) & 0xff;
    _data[_position + 4] = (value >> 0) & 0xff;
    _position += 5;
}

void ZigBeeStream::writeUInt48Le(uint64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _position += 6;
}

void ZigBeeStream::writeUInt48Be(uint64_t value) {
    _data[_position + 0] = (value >> 40) & 0xff;
    _data[_position + 1] = (value >> 32) & 0xff;
    _data[_position + 2] = (value >> 24) & 0xff;
    _data[_position + 3] = (value >> 16) & 0xff;
    _data[_position + 4] = (value >> 8) & 0xff;
    _data[_position + 5] = (value >> 0) & 0xff;
    _position += 6;
}

void ZigBeeStream::writeUInt56Le(uint64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _data[_position + 6] = (value >> 48) & 0xff;
    _position += 7;
}

void ZigBeeStream::writeUInt56Be(uint64_t value) {
    _data[_position + 0] = (value >> 48) & 0xff;
    _data[_position + 1] = (value >> 40) & 0xff;
    _data[_position + 2] = (value >> 32) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 16) & 0xff;
    _data[_position + 5] = (value >> 8) & 0xff;
    _data[_position + 6] = (value >> 0) & 0xff;
    _position += 7;
}

void ZigBeeStream::writeUInt64Le(uint64_t value) {
    _data[_position + 0] = (value >> 0) & 0xff;
    _data[_position + 1] = (value >> 8) & 0xff;
    _data[_position + 2] = (value >> 16) & 0xff;
    _data[_position + 3] = (value >> 24) & 0xff;
    _data[_position + 4] = (value >> 32) & 0xff;
    _data[_position + 5] = (value >> 40) & 0xff;
    _data[_position + 6] = (value >> 48) & 0xff;
    _data[_position + 7] = (value >> 56) & 0xff;
    _position += 8;
}

void ZigBeeStream::writeUInt64Be(uint64_t value) {
    _data[_position + 0] = (value >> 56) & 0xff;
    _data[_position + 1] = (value >> 48) & 0xff;
    _data[_position + 2] = (value >> 40) & 0xff;
    _data[_position + 3] = (value >> 32) & 0xff;
    _data[_position + 4] = (value >> 24) & 0xff;
    _data[_position + 5] = (value >> 16) & 0xff;
    _data[_position + 6] = (value >> 8) & 0xff;
    _data[_position + 7] = (value >> 0) & 0xff;
    _position += 8;
}

// GENERATION END

float ZigBeeStream::readSingle() {
    float result;
    memcpy(&result, _data, sizeof(float));
    _position += sizeof(float);
    return result;
}

double ZigBeeStream::readDouble() {
    double result;
    memcpy(&result, _data, sizeof(double));
    _position += sizeof(double);
    return result;
}

void ZigBeeStream::writeSingle(float value) {
    memcpy(_data, &value, sizeof(float));
    _position += sizeof(float);
}

void ZigBeeStream::writeDouble(double value) {
    memcpy(_data, &value, sizeof(double));
    _position += sizeof(double);
}

Buffer ZigBeeStream::readOctstr() { return readOctstrData(readUInt8()); }

Buffer ZigBeeStream::readOctstr16Le() { return readOctstrData(readUInt16Le()); }

Buffer ZigBeeStream::readOctstrData(int length) {
    auto result = Buffer(_data + _position, length);
    _position += length;
    return result;
}

void ZigBeeStream::writeOctstr(const Buffer& value) {
    int length = value.length();
    if (length > 254) {
        ESP_LOGE(TAG, "Writing octstr of more than 254 bytes");
    }
    writeUInt8(length);
    writeOctstrData(value);
}

void ZigBeeStream::writeOctstr16Le(const Buffer& value) {
    int length = value.length();
    writeUInt16Le(length);
    writeOctstrData(value);
}

void ZigBeeStream::writeOctstrData(const Buffer& value) {
    value.getBytes(_data + _position);
    _position += value.length();
}

std::string ZigBeeStream::readString() { return readStringData(readUInt8()); }

std::string ZigBeeStream::readString16Le() { return readStringData(readUInt16Le()); }

std::string ZigBeeStream::readStringData(int length) {
    auto result = std::string();

    // See
    // https://github.com/arduino/ArduinoCore-sam/blob/790ff2c852bf159787a9966bddee4d9f55352d15/cores/arduino/WString.cpp#L159.
    // Reserve reserves the maximum string length, not the number of bytes.
    // It'll actually allocate one extra byte.
    result.reserve(length);

    for (int i = 0; i < length; i++) {
        result += (char)readUInt8();
    }
    return result;
}

void ZigBeeStream::writeString(const std::string& value) {
    int length = value.length();
    if (length > 254) {
        ESP_LOGE(TAG, "Writing string of more than 254 bytes");
    }
    writeUInt8(length);
    writeStringData(value);
}

void ZigBeeStream::writeString16Le(const std::string& value) {
    int length = value.length();
    writeUInt16Le(length);
    writeStringData(value);
}

void ZigBeeStream::writeStringData(const std::string& value) {
    int length = value.length();
    memcpy(_data + _position, value.c_str(), length);
    _position += length;
}
