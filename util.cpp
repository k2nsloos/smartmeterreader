#include "util.h"
#include <Arduino.h>
#include <stdarg.h>

uint8_t concat_str(char** buf, const char* end, const char* str_p)
{
    uint8_t max_len = end - *buf;
    uint8_t len = strlen_P(str_p);
    if (len >= max_len) len = max_len - 1;
    memcpy_P(*buf, str_p, len);
    *buf += len;
    **buf = 0;
    return len;
}

uint8_t print_number(char **buf, const char* end, int16_t value, uint8_t prec)
{
    char tmp[8 * sizeof(long) + 2];
    char *str = tmp + sizeof(tmp);
    bool write_minus = false;

    if (value < 0) {
        value = -value;
        write_minus = true;
    }

    if (prec) {
        do {
            char c = value % 10;
            value /= 10;
            *--str = c + '0';
        } while(--prec);

        *--str = '.';
    }

    do {
        char c = value % 10;
        value /= 10;
        *--str = c + '0';
    } while(value);

    if (write_minus) *--str = '-';

    uint8_t len = sizeof(tmp) - (str - tmp);
    uint8_t max_len = end - *buf;
    if (len >= max_len) len = max_len - 1;

    memcpy(*buf, str, len);
    *buf += len;
    **buf = 0;

    return len;
}

static void write_P(const char* begin, uint8_t max_length)
{
    const char* end = begin + max_length;
    while(begin < end)
    {
        uint8_t ch = pgm_read_byte(begin++);
        if (ch == 0) break;
        Serial.write(ch);
    }
}

static void vlog(const char* tpl_p, va_list ap)
{
    bool is_escape = false;
    const char* begin_p = (const char*)tpl_p;

    char ch;
    while ((ch = pgm_read_byte(tpl_p)) != 0) {
        if (is_escape)
        {
            if (ch == 'd') {
                Serial.print(va_arg(ap, int));
            } else if (ch == 's') {
                write_P(va_arg(ap, const char*), UINT8_MAX);
            } else if (ch == '%') {
                Serial.write('%');
            }
            is_escape = false;
            begin_p = tpl_p + 1;
        }
        else if (ch == '%')
        {
            is_escape = true;
            write_P(begin_p, tpl_p - begin_p);
        }
        ++tpl_p;
    }

    if (begin_p != tpl_p) write_P(begin_p, tpl_p - begin_p);
    Serial.print(F("\r\n"));


}

void log(const __FlashStringHelper* tpl_p, ...)
{
    va_list ap;
    va_start(ap, tpl_p);
    vlog((const char*)tpl_p, ap);
    va_end(ap);
}


void log(const char* tpl_p, ...)
{
    va_list ap;
    va_start(ap, tpl_p);
    vlog((const char*)tpl_p, ap);
    va_end(ap);
}

