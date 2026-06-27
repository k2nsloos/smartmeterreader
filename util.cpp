#include "util.h"
#include "Config.h"
#include <Arduino.h>
#include <stdarg.h>

static const char s_log_level_debug[] PROGMEM = "[D]";
static const char s_log_level_info[] PROGMEM = "   ";
static const char s_log_level_warn[] PROGMEM = "[W]";
static const char s_log_level_error[] PROGMEM = "[E]";

static const char* s_log_prefix_p[] = {
    s_log_level_debug,
    s_log_level_info,
    s_log_level_warn,
    s_log_level_error
};

static Print* s_output = &Serial;

static char to_hex(uint8_t nibble) {
    return nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
}

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
        s_output->write(ch);
    }
}

static void vlog(LogLevel level, const char* tpl_p, va_list ap)
{
    const char* prefix_p = (const char*)pgm_read_ptr(&s_log_prefix_p[level]);
    write_P(prefix_p, 3);
    s_output->print(' ');

    bool is_escape = false;
    const char* begin_p = (const char*)tpl_p;

    char ch;
    while ((ch = pgm_read_byte(tpl_p)) != 0) {
        if (is_escape)
        {
            if (ch == 'l') {
                s_output->print(va_arg(ap, long));
                ++tpl_p; // skip d
            } else if (ch == 'd') {
                s_output->print(va_arg(ap, int));
            } else if (ch == 's') {
                write_P(va_arg(ap, const char*), UINT8_MAX);
            } else if (ch == '%') {
                s_output->write('%');
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
    s_output->print(F("\r\n"));
}

void log_set_output(Print* output)
{
    s_output = output;
}

void log(LogLevel level, const __FlashStringHelper* tpl_p, ...)
{
    if (ENABLE_LOGGING == LOG_MODE_DISABLED) return;
    if (level < LOG_LEVEL) return;

    va_list ap;
    va_start(ap, tpl_p);
    vlog(level, (const char*)tpl_p, ap);
    va_end(ap);
}


void log(LogLevel level, const char* tpl_p, ...)
{
    if (ENABLE_LOGGING == LOG_MODE_DISABLED) return;
    if (level < LOG_LEVEL) return;

    va_list ap;
    va_start(ap, tpl_p);
    vlog(level, (const char*)tpl_p, ap);
    va_end(ap);
}

void log_buf(LogLevel level, const char* module, const uint8_t* buf, size_t length)
{
    if (ENABLE_LOGGING == LOG_MODE_DISABLED) return;
    if (level < LOG_LEVEL) return;

    char tmp[301];
    size_t w_idx = 0;
    size_t r_idx = 0;

    while (w_idx < 300 && r_idx < length) {
        tmp[w_idx++] = to_hex(buf[r_idx] >> 4);
        tmp[w_idx++] = to_hex(buf[r_idx++] & 0x0F);
        tmp[w_idx++] = ' ';
    }

    tmp[w_idx] = 0;
    log(level, "%s: count: %d, frame: %s", length, tmp);
}

#include "sm_common.h"

void log_meter_values(LogLevel level, const char* module, const sm_values_* v)
{
    if (ENABLE_LOGGING == LOG_MODE_DISABLED) return;
    if (level < LOG_LEVEL) return;

    log(level, "%s: received frame:", module);
    log(level, "  power:     %ld %ld %ld (W)", (long)v->power_w[0], (long)v->power_w[1], (long)v->power_w[2]);
    log(level, "  current:   %ld %ld %ld (mA)", (long)v->current_ma[0], (long)v->current_ma[1], (long)v->current_ma[2]);
    log(level, "  potential: %ld %ld %ld (mV)", (long)v->potential_mv[0], (long)v->potential_mv[1], (long)v->potential_mv[2]);
    log(level, "  energy:    %ld %ld (Wh)", (long)v->import_wh, (long)v->export_wh);
}