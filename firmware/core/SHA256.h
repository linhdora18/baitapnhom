#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
using namespace std;

namespace ss {
    void SHA256(const uint8_t* data, size_t len, uint8_t* out[32]);
    void hmac_SHA256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]);
    string to_hex(const uint8_t* data, size_t n);
    bool ct_equal(const uint8_t* a, const uint8_t* b, size_t n);      // So sánh thời gian cố định (Constant-time compare)
}