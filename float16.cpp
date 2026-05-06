#include <iostream>
#include <cstring>
using namespace std;

uint16_t toFloat16(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));

    uint16_t sign     = (bits >> 16) & 0x8000;
    int32_t  exponent = ((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = (bits >> 13) & 0x3FF;

    if ((bits & 0x7F800000) == 0x7F800000)
        return sign | 0x7C00 | (mantissa ? 0x200 : 0);

    if (exponent >= 31) return sign | 0x7C00;
    if (exponent <= 0)  return sign;

    return sign | (exponent << 10) | mantissa;
}

float fromFloat16(uint16_t value) {
    uint32_t sign     = (value & 0x8000) << 16;
    uint32_t exponent = (value & 0x7C00) >> 10;
    uint32_t mantissa = (value & 0x03FF);

    uint32_t bits;

    if (exponent == 0) {
        bits = sign;
    } else if (exponent == 31) {                         
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    float result;
    memcpy(&result, &bits, sizeof(float));
    return result;
}