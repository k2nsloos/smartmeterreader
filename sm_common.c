#include "sm_common.h"
#include <time.h>
#include <stdlib.h>


#define UNIX_EPOCH 62167132800


static int32_t year2days(int year)
{
    int base = (year - 1) / 4;
    int corr1 = base / 25;
    int corr2 = corr1 / 4;
    int leap_years = base - corr1 + corr2;
    return (int32_t)year * 365 + leap_years;
}

static bool is_leap_year(int year)
{
    if ((year %= 4) != 0) return false;
    if ((year %= 25) != 0) return true;
    return year % 4 != 0;
}

static int month2days(int month, bool is_leap)
{
    int days = (214 * month - 211) / 7;
    if (month > 2) days -= is_leap ? 1 : 2;
    return days;
}

static int inv_year2days(int32_t days)
{
    int cur = days / 365;
    int32_t error = year2days(cur) - days;
    int32_t prev_error;

    do {
        cur -= error / 366;
        prev_error = error;
        error = year2days(cur) - days;
    } while (prev_error != error);

    if (error > 0) --cur;
    return cur;
}

static int inv_month2days(int days, bool is_leap)
{
    int cur = 1 + days / 29;
    int error = month2days(cur, is_leap) - days;
    int prev_error;

    do {
        cur -= error / 31;
        prev_error = error;
        error = month2days(cur, is_leap) - days;
    } while (prev_error != error);

    if (error > 0) --cur;
    return cur;
}

static int64_t sec_since_epoch(const sm_datetime_s* dt)
{
    int32_t day_offset = 60 * ((int32_t)60 * dt->hour + dt->min) + dt->sec;
    int32_t days_year_epoch = year2days(dt->year);
    int32_t days_month_year = month2days(dt->month, is_leap_year(dt->year));
    return day_offset + (int64_t)HOURS_PER_DAY * SECONDS_PER_HOUR * (days_year_epoch + days_month_year + dt->day - 1);
}

int64_t sm_datetime_as_timestamp(const sm_datetime_s* dt)
{
    return sec_since_epoch(dt) - UNIX_EPOCH;
}

void sm_datetime_from_timestamp(sm_datetime_s* dt, int64_t timestamp)
{
    timestamp += UNIX_EPOCH;
    int32_t days = timestamp / SECONDS_PER_DAY;
    
    timestamp %= SECONDS_PER_DAY;
    
    dt->sec = timestamp % 60;
    timestamp /= 60;

    dt->min = timestamp % 60;
    timestamp /= 60;

    dt->hour = timestamp % 24;
    timestamp /= 24;

    dt->year = inv_year2days(days);
    days -= year2days(dt->year);

    bool is_leap = is_leap_year(dt->year);
    dt->month = inv_month2days(days, is_leap);
    days -= month2days(dt->month, is_leap);

    dt->day = days + 1;
    dt->day_offset = 60 * ((int32_t)60 * dt->hour + dt->min) + dt->sec;
    dt->is_dst = false;
}

int64_t sm_datetime_diff(const sm_datetime_s* begin, const sm_datetime_s* end)
{
    return sec_since_epoch(end) - sec_since_epoch(begin);
}