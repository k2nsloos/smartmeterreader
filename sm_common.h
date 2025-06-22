#ifndef INCLUDED_SM_COMMON_H_
#define INCLUDED_SM_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SM_READ_ERROR SIZE_MAX
#define SMART_METER_TOKENIZER_BUF_SIZE 32
#define SMART_METER_READ_BUF_SIZE 64

#define HOURS_PER_DAY 24
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400

typedef struct {
    uint32_t day_offset;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    bool is_dst;
} sm_datetime_s;

typedef struct {
    sm_datetime_s timestamp;
    int32_t power_w[3];
    int64_t import_wh;
    int64_t export_wh;
} sm_values_s;

typedef void (*sm_frame_callback_f)(void*, const sm_values_s*);

int64_t sm_datetime_as_timestamp(const sm_datetime_s* dt);
void sm_datetime_from_timestamp(sm_datetime_s* dt, int64_t timestamp);
int64_t sm_datetime_diff(const sm_datetime_s* begin, const sm_datetime_s* end);

#ifdef __cplusplus
}
#endif

#endif