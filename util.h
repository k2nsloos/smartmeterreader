#ifndef INCLUDED_UTIL_H_
#define INCLUDED_UTIL_H_

#include <stdint.h>
#include <Arduino.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))


typedef int16_t (*getter_f)(const void*);

template<typename T, typename U, U (T::*func)() const>
static int16_t get_member(const void* ctx)
{
    const T* ptr = reinterpret_cast<const T*>(ctx);
    return static_cast<U>((ptr->*func)());
}

uint8_t concat_str(char** buf, const char* end, const char* str_p);
uint8_t print_number(char **buf, const char* end, int16_t value, uint8_t prec);
void log(const __FlashStringHelper* tpl, ...);
void log(const char* tpl, ...);
void log_buf(const uint8_t* buf, size_t length);

struct sm_values_;
void log_meter_values(const char* module, const sm_values_* v);

#endif // INCLUDED_UTIL_H_