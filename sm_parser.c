#include "sm_parser.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#define PARSE_ERROR SIZE_MAX

typedef sm_token_e (*sm_state_action_f)(sm_tokenizer_t* t, char ch);
static sm_token_e handle_default(sm_tokenizer_t* t, char ch);
static sm_token_e handle_parse_key(sm_tokenizer_t* t, char ch);
static sm_token_e handle_parse_value(sm_tokenizer_t* t, char ch);
static sm_token_e handle_parse_end_of_frame(sm_tokenizer_t* t, char ch);
static sm_token_e handle_parse_frame_header(sm_tokenizer_t* t, char ch);

static const sm_state_action_f s_state_action_table[] = {
    [SM_TOKENIZER_DEFAULT] = handle_default,
    [SM_TOKENIZER_PARSE_KEY] = handle_parse_key,
    [SM_TOKENIZER_PARSE_VALUE] = handle_parse_value,
    [SM_TOKENIZER_PARSE_END_OF_FRAME] = handle_parse_end_of_frame,
    [SM_TOKENIZER_PARSE_FRAME_HEADER] = handle_parse_frame_header
};

static bool handle_timestamp(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);
static bool handle_power_import(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);
static bool handle_power_export(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);
static bool handle_energy_export(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);
static bool handle_energy_import(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);

static const sm_parser_entry_s s_ibis_keys_parsers[] = {
    { "0-0:1.0.0", handle_timestamp, 0 },
    { "1-0:1.8.1", handle_energy_import, 0 },
    { "1-0:1.8.2", handle_energy_import, 0 },
    { "1-0:2.8.1", handle_energy_export, 1 },
    { "1-0:2.8.2", handle_energy_export, 1 },
    { "1-0:1.7.0", handle_power_import, 0 },
    { "1-0:2.7.0", handle_power_export, 0 },
    { "1-0:21.7.0", handle_power_import, 1 },
    { "1-0:22.7.0", handle_power_export, 1 },
    { "1-0:41.7.0", handle_power_import, 2 },
    { "1-0:42.7.0", handle_power_export, 2 },
    { "1-0:61.7.0", handle_power_import, 3 },
    { "1-0:62.7.0", handle_power_export, 3 },
};

static void push_ch(sm_tokenizer_t* t, char ch)
{
    if (t->idx < sizeof(t->buf)) t->buf[t->idx++] = ch;
}

static void accept(sm_tokenizer_t* t, sm_token_e token)
{
    memcpy(t->last_token_buf, t->buf, t->idx);
    t->last_token_buf[t->idx] = 0;
    t->last_token = token;
    t->idx = 0;
}

static sm_token_e handle_default(sm_tokenizer_t* t, char ch)
{
    switch(ch) {
    case '/':
        t->state = SM_TOKENIZER_PARSE_FRAME_HEADER;
        return SM_TOKEN_NEED_MORE_INPUT;

    case '!':
        t->state = SM_TOKENIZER_PARSE_END_OF_FRAME;
        return SM_TOKEN_NEED_MORE_INPUT;

    case '(':
        t->state = SM_TOKENIZER_PARSE_VALUE;
        return SM_TOKEN_NEED_MORE_INPUT;

    case ')':
        t->state = SM_TOKENIZER_DEFAULT;
        return SM_TOKEN_ERROR;

    case 0:
    case ' ':
    case '\r':
    case '\n':
    case '\t':
        return SM_TOKEN_NEED_MORE_INPUT;

    default:
        t->state = SM_TOKENIZER_PARSE_KEY;
        push_ch(t, ch);
        return SM_TOKEN_NEED_MORE_INPUT;
    }
}

static sm_token_e handle_parse_frame_header(sm_tokenizer_t* t, char ch)
{
    switch(ch) {
    case '/':
    case '!':
        handle_default(t, ch);
        return SM_TOKEN_ERROR;

    case '\r':
    case '\n':
        t->state = SM_TOKENIZER_DEFAULT;
        return SM_TOKEN_FRAME_HEADER;

    default:
        push_ch(t, ch);
        break;
    }

    return SM_TOKEN_NEED_MORE_INPUT;
}

static sm_token_e handle_parse_key(sm_tokenizer_t* t, char ch)
{
    switch(ch) {
    case 0:
    case '/':
    case '!':
    case ')':
        handle_default(t, ch);
        return SM_TOKEN_ERROR;

    case '(':
        t->state = SM_TOKENIZER_PARSE_VALUE;
        return SM_TOKEN_KEY;

    case ' ':
    case '\t':
    case '\r':
    case '\n':
        t->state = SM_TOKENIZER_DEFAULT;
        return SM_TOKEN_KEY;

    default:
        push_ch(t, ch);
        return SM_TOKEN_NEED_MORE_INPUT;
    }
}

static sm_token_e handle_parse_value(sm_tokenizer_t* t, char ch)
{
    switch(ch) {
    case 0:
    case '/':
    case '!':
    case '(':
        handle_default(t, ch);
        return SM_TOKEN_ERROR;

    case ')':
        t->state = SM_TOKENIZER_DEFAULT;
        return SM_TOKEN_VALUE;

    case ' ':
    case '\t':
    case '\r':
    case '\n':
        return SM_TOKEN_NEED_MORE_INPUT;

    default:
        push_ch(t, ch);
        return SM_TOKEN_NEED_MORE_INPUT;
    }
}

static sm_token_e handle_parse_end_of_frame(sm_tokenizer_t* t, char ch)
{
    if (ch < '0' || ch > '9') {
        handle_default(t, ch);
        return SM_TOKEN_FRAME_FOOTER;
    }

    push_ch(t, ch);
    return t->idx < 4 ? SM_TOKEN_NEED_MORE_INPUT : SM_TOKEN_FRAME_FOOTER;
}


static void sm_tokenizer_reset(sm_tokenizer_t* t)
{
    *t = (sm_tokenizer_t) {
        .state = SM_TOKENIZER_DEFAULT,
        .last_token = SM_TOKEN_NEED_MORE_INPUT,
        .last_token_buf = { 0 },
        .buf = { 0 },
        .idx = 0
    };
}


void sm_tokenizer_init(sm_tokenizer_t* t)
{
    sm_tokenizer_reset(t);
}

sm_token_e sm_tokenizer_last_token(const sm_tokenizer_t* t)
{
    return t->last_token;
}

const char* sm_tokenizer_last_token_str(const sm_tokenizer_t* t)
{
    return t->last_token_buf;
}

sm_token_e sm_tokenizer_parse_ch(sm_tokenizer_t* t, char ch)
{
    sm_token_e result = s_state_action_table[t->state](t, ch);
    if (result != SM_TOKEN_NEED_MORE_INPUT) accept(t, result);
    return result;
}

sm_token_e sm_tokenizer_finalize(sm_tokenizer_t* t)
{
    return sm_tokenizer_parse_ch(t, 0);
}

static int64_t parse_flt(const char** buf, int decimals)
{
    const char* ch = *buf;
    bool is_negative = false;
    int64_t result = 0;

    if (*ch == '-') {
        is_negative = true;
    }

    while (*ch >= '0' && *ch <= '9') {
        result *= 10;
        result += (*ch++ - '0');
    }

    if (*ch == '.') {
        ++ch;
        while(*ch >= '0' && *ch <= '9') {
            if (decimals == 0) continue;

            --decimals;
            result *= 10;
            result += (*ch++ - '0');
        }
    }

    while(decimals-- > 0) result *= 10;

    *buf = ch;
    return is_negative ? -result : result;
}

static bool parse_subint(const char **buf, size_t digits, uint8_t* out_result)
{
    uint8_t result = 0;
    const char* ch = *buf;
    while (digits--) {
        if (*ch < '0' || *ch > '9') return false;

        result *= 10;
        result += (*ch++ - '0');
    }

    *out_result = result;
    *buf = ch;
    return true;
}

static bool str_starts_with(const char* str, const char* substr)
{
    while(*substr && tolower(*str) == tolower(*substr)) {
        ++str;
        ++substr;
    }
    return *substr == 0;
}

static bool str_is_substring(const char *tpl, const char *substring)
{
    while(*tpl) {
        if (str_starts_with(tpl, substring)) return true;
        ++tpl;
    }
    return false;
}

static bool parse_energy(const char* value, int64_t *out_energy)
{
    const char* cur = value;
    int64_t energy = parse_flt(&cur, 3);
    bool is_wh = str_is_substring(cur, "*Wh");
    bool is_kwh = str_is_substring(cur, "*kWh");
    if (is_wh) energy /= 1000;
    if (is_wh || is_kwh) *out_energy = energy;
    return is_wh || is_kwh;
}

static bool parse_power(const char* value, int32_t *out_power)
{
    const char* cur = value;
    int64_t power = parse_flt(&cur, 3);
    bool is_w = str_is_substring(cur, "*W");
    bool is_kw = str_is_substring(cur, "*kW");
    if (is_w) power /= 1000;
    if (is_w || is_kw) *out_power = power;
    return is_w || is_kw;
}

static bool handle_timestamp(sm_parser_t* p, uint8_t phase, size_t value_idx, const char* value)
{
    uint8_t year;

    if (!parse_subint(&value, 2, &year)) return false;
    if (!parse_subint(&value, 2, &p->timestamp.month)) return false;
    if (!parse_subint(&value, 2, &p->timestamp.day)) return false;
    if (!parse_subint(&value, 2, &p->timestamp.hour)) return false;
    if (!parse_subint(&value, 2, &p->timestamp.min)) return false;
    if (!parse_subint(&value, 2, &p->timestamp.sec)) return false;
    if (*value != 'S' && *value != 'W') return false;

    p->timestamp.is_dst = *value == 'S';
    p->timestamp.year = 2000 + year;
    p->timestamp.day_offset = (uint32_t)p->timestamp.sec
                                    + SECONDS_PER_MINUTE * p->timestamp.min
                                    + SECONDS_PER_HOUR * p->timestamp.hour;
    return true;
}

static bool handle_power_import(sm_parser_t* p, uint8_t phase, size_t value_idx, const char* value)
{
    int32_t power = 0;
    if (value_idx != 0) return false;
    if (!parse_power(value, &power)) return false;
    if (phase > 0) p->is_lx_power_present = true;
    p->power[phase] += power;
    return true;
}

static bool handle_power_export(sm_parser_t* p, uint8_t phase, size_t value_idx, const char* value)
{
    int32_t power = 0;
    if (value_idx != 0) return false;
    if (!parse_power(value, &power)) return false;
    if (phase > 0) p->is_lx_power_present = true;
    p->power[phase] -= power;
    return true;
}

static bool handle_energy_import(sm_parser_t* p, uint8_t phase, size_t value_idx, const char* value)
{
    int64_t energy = 0;
    if (value_idx != 0) return false;
    if (!parse_energy(value, &energy)) return false;
    p->import_wh += energy;
    return true;
}

static bool handle_energy_export(sm_parser_t* p, uint8_t phase, size_t value_idx, const char* value)
{
    int64_t energy = 0;
    if (value_idx != 0) return false;
    if (!parse_energy(value, &energy)) return false;
    p->export_wh += energy;
    return true;
}

static void handle_frame_start(sm_parser_t* p)
{
    memset(p->power, 0, sizeof(p->power));
    p->import_wh = 0;
    p->export_wh = 0;
    p->is_lx_power_present = false;
    p->model[0] = 0;
}

static void handle_frame_end(sm_parser_t* p)
{
    if (!p->is_lx_power_present) p->power[1] = p->power[0];
    memcpy(p->last_frame.power_w, &p->power[1], sizeof(p->last_frame.power_w));
    p->last_frame.timestamp = p->timestamp;
    p->last_frame.import_wh = p->import_wh;
    p->last_frame.export_wh = p->export_wh;

    if (p->on_frame_parsed) p->on_frame_parsed(p->arg, &p->last_frame);
}

static const sm_parser_entry_s* find_ibis_key(sm_parser_t* p, const char* token_str)
{
    size_t count = sizeof(s_ibis_keys_parsers) / sizeof(s_ibis_keys_parsers[0]);
    for (size_t idx = 0; idx < count; ++idx) {
        const sm_parser_entry_s* entry = &s_ibis_keys_parsers[idx];
        if (strcmp(token_str, entry->ibis_key) == 0) return entry;
    }
    return NULL;
}

static bool sm_parser_parse_token(sm_parser_t* p, sm_token_e token)
{
    const char* tokenStr = sm_tokenizer_last_token_str(&p->tokenizer);
    if (token == SM_TOKEN_NEED_MORE_INPUT) return true;

    switch(p->state) {
    case STATE_PARSE_HEADER:
        if (token == SM_TOKEN_FRAME_HEADER) {
            handle_frame_start(p);
            strcpy(p->model, tokenStr);
            p->state = STATE_PARSE_KEY;
        }
        return true;    // Any input is considered valid until start of header
        break;

    case STATE_PARSE_KEY:
        if (token == SM_TOKEN_ERROR) return false;

        if (token == SM_TOKEN_KEY) {
            p->active_key = find_ibis_key(p, tokenStr);
            p->value_idx = 0;
            p->state = STATE_PARSE_VALUES;
            return true;
        }

        if (token == SM_TOKEN_FRAME_FOOTER) {
            p->state = STATE_PARSE_HEADER;
            return true;
        }
        break;

    case STATE_PARSE_VALUES:
        if (token == SM_TOKEN_ERROR) return false;

        if (token == SM_TOKEN_KEY) {
            if (p->value_idx != 0) {
                p->active_key = find_ibis_key(p, tokenStr);
                p->value_idx = 0;
                return true;
            }
        }

        if (token == SM_TOKEN_VALUE) {
            bool ok = true;
            if (p->active_key) {
                ok = p->active_key->parser(p, p->active_key->phase, p->value_idx, tokenStr);
            }
            ++p->value_idx;
            return ok;
        }

        if (token == SM_TOKEN_FRAME_FOOTER) {
            if (p->value_idx != 0) {
                p->state = STATE_PARSE_HEADER;
                handle_frame_end(p);
                return true;
            }
        }
    }

    return false;
}


bool sm_parser_parse_ch(sm_parser_t* p, char ch)
{
    sm_token_e token = sm_tokenizer_parse_ch(&p->tokenizer, ch);
    bool ok = sm_parser_parse_token(p, token);
    if (!ok) p->state = STATE_PARSE_HEADER;
    return ok;
}

void sm_parser_init(sm_parser_t* p)
{
    memset(p, 0x00, sizeof(sm_parser_t));
    sm_tokenizer_init(&p->tokenizer);
}

void sm_parser_set_frame_callback(sm_parser_t* p, void (*on_frame_parsed)(void*, const sm_values_s*), void *arg)
{
    p->on_frame_parsed = on_frame_parsed;
    p->arg = arg;
}

bool sm_parser_parse_str(sm_parser_t* p, const char* str, size_t length)
{
    bool result = true;
    while (length--) {
        if (!sm_parser_parse_ch(p, *str++)) result = false;
    }
    return result;
}

bool sm_parser_finalize(sm_parser_t* p)
{
    return sm_parser_parse_ch(p, 0);
}

void sm_parser_reset(sm_parser_t* p)
{
    sm_tokenizer_reset(&p->tokenizer);
    memset(&p->last_frame, 0x00, sizeof(p->last_frame));
    p->state = STATE_PARSE_HEADER;
}

const sm_values_s* sm_parser_get_values(const sm_parser_t* p)
{
    return &p->last_frame;
}

const char* sm_parser_get_model(const sm_parser_t* p)
{
    return p->model;
}