#ifndef INCLUDED_SM_PARSER_H_
#define INCLUDED_SM_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sm_common.h"

typedef struct sm_tokenizer_ sm_tokenizer_t;
typedef struct sm_parser_ sm_parser_t;

typedef enum sm_tokenizer_state_ {
    SM_TOKENIZER_DEFAULT,
    SM_TOKENIZER_PARSE_KEY,
    SM_TOKENIZER_PARSE_VALUE,
    SM_TOKENIZER_PARSE_END_OF_FRAME,
    SM_TOKENIZER_PARSE_FRAME_HEADER
} sm_tokenizer_state_e;

typedef enum sm_token_ {
    SM_TOKEN_NEED_MORE_INPUT,
    SM_TOKEN_FRAME_HEADER,
    SM_TOKEN_KEY,
    SM_TOKEN_VALUE,
    SM_TOKEN_FRAME_FOOTER,
    SM_TOKEN_ERROR,
} sm_token_e;

struct sm_tokenizer_ {
    sm_tokenizer_state_e state;
    sm_token_e last_token;
    char last_token_buf[SMART_METER_TOKENIZER_BUF_SIZE];
    char buf[SMART_METER_TOKENIZER_BUF_SIZE - 1];
    uint8_t idx;
};

typedef enum sm_parser_state_ {
    STATE_PARSE_HEADER,
    STATE_PARSE_KEY,
    STATE_PARSE_VALUES
} sm_parser_state_e;

typedef struct sm_parser_entry_ {
    const char* ibis_key;
    bool (*parser)(sm_parser_t* t, uint8_t phase, size_t value_idx, const char* value);
    uint8_t phase;
} sm_parser_entry_s;

struct sm_parser_ {
    sm_tokenizer_t tokenizer;
    sm_frame_callback_f on_frame_parsed;
    void *arg;

    const sm_parser_entry_s* active_key;
    sm_values_s last_frame;
    uint8_t value_idx;
    sm_parser_state_e state;

    char model[SMART_METER_TOKENIZER_BUF_SIZE];
    sm_datetime_s timestamp;
    int32_t power[4];
    int64_t import_wh;
    int64_t export_wh;
    bool is_lx_power_present : 1;
};

void sm_parser_init(sm_parser_t* p);
void sm_parser_set_frame_callback(sm_parser_t* p, sm_frame_callback_f on_frame_parsed, void *arg);
void sm_parser_reset(sm_parser_t* p);

bool sm_parser_parse_ch(sm_parser_t* p, char ch);
bool sm_parser_parse_str(sm_parser_t* p, const char* str, size_t length);
bool sm_parser_finalize(sm_parser_t* p);

const sm_values_s* sm_parser_get_values(const sm_parser_t* p);
const char* sm_parser_get_model(const sm_parser_t* p);

#ifdef __cplusplus
}
#endif

#endif