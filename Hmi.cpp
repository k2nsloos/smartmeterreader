#include "Hmi.h"
#include <Arduino.h>
#include <stdint.h>
#include "util.h"

const int Hmi::s_blink_count[] = {
    [NO_WIFI_CONNECTION] = 1,
    [NO_SM_CONNECTION] = 2,
};

const char* const Hmi::s_error_names[] = {
    [NO_WIFI_CONNECTION] = "wifi disconnected",
    [NO_SM_CONNECTION] = "smartmeter disconnected"
};

const char* const Hmi::s_state_names[] = {
    [STATE_INTRA_PATTERN_DELAY] = "STATE_INTRA_PATTERN_DELAY",
    [STATE_PATTERN_ON] = "STATE_PATTERN_ON",
    [STATE_PATTERN_OFF] = "STATE_PATTERN_OFF",
    [STATE_NO_ERROR_ENTER] = "STATE_NO_ERROR_ENTER",
    [STATE_NO_ERROR_OFF] = "STATE_NO_ERROR_OFF",
};


Hmi::Hmi(uint8_t pin)
{
    _pin = pin;
    _active_errors = (1u << NUMBER_OF_ERRORS) - 1;
}

void Hmi::begin()
{
    _error_idx = NUMBER_OF_ERRORS;
    pinMode(_pin, OUTPUT);
    set_led(false);
}

void Hmi::loop()
{
    unsigned long now = millis();

    switch (_state) {
        case STATE_INTRA_PATTERN_DELAY:
            if (now - _state_entry_time >= intra_delay_ms) {
                bool has_error = select_error_to_show();
                set_state(has_error ? STATE_PATTERN_ON : STATE_NO_ERROR_ENTER);
            }
            break;

        case STATE_PATTERN_ON:
            if (now - _state_entry_time >= on_time_ms) {
                --_blinks_remaining;
                set_state(STATE_PATTERN_OFF);
            }
            break;

        case STATE_PATTERN_OFF:
            if (now - _state_entry_time >= off_time_ms) {
                bool is_done = _blinks_remaining == 0;
                set_state(is_done ? STATE_INTRA_PATTERN_DELAY : STATE_PATTERN_ON);
            }
            break;

        case STATE_NO_ERROR_ENTER:
            if (_active_errors != 0 && now - _state_entry_time >= intra_delay_ms) {
                bool has_error = select_error_to_show();
                set_state(has_error ? STATE_PATTERN_ON : STATE_NO_ERROR_ENTER);
            } else if (now - _state_entry_time >= no_error_on_ms) {
                set_state(STATE_NO_ERROR_OFF);
            }
            break;

        case STATE_NO_ERROR_OFF:
            if (_active_errors != 0) {
                bool has_error = select_error_to_show();
                set_state(has_error ? STATE_PATTERN_ON : STATE_NO_ERROR_ENTER);
            }
            break;
    }
}

void Hmi::set_error(hmi_error_e error, bool is_error_active)
{
    unsigned mask = 1u << error;
    bool was_error_active = _active_errors & mask;
    if (is_error_active == was_error_active) return;

    log("hmi: %s error %s", s_error_names[error], is_error_active ? "active" : "cleared");

    if (is_error_active) {
        _active_errors |= mask;
    } else {
        _active_errors &= ~mask;
    }

    if (_active_errors == 0) log("hmi: all ok");
}

void Hmi::set_state(State state)
{
    _state = state;
    _state_entry_time = millis();

    switch (_state) {
        case STATE_INTRA_PATTERN_DELAY:
            set_led(false);
            break;

        case STATE_PATTERN_ON:
            set_led(true);
            break;

        case STATE_PATTERN_OFF:
            set_led(false);
            break;

        case STATE_NO_ERROR_ENTER:
            set_led(true);
            break;

        case STATE_NO_ERROR_OFF:
            set_led(false);
            break;
    }

    //log("hmi: entered state %s", s_state_names[state]);
}

void Hmi::set_led(bool is_on)
{
    digitalWrite(_pin, is_on ? LOW : HIGH);
}

bool Hmi::select_error_to_show()
{
    uint8_t org = _error_idx;
    if (!_active_errors) return false;

    do {
        if (++_error_idx >= NUMBER_OF_ERRORS) _error_idx = 0;
        unsigned mask = 1u << _error_idx;

        if (mask & _active_errors) {
            //log("hmi: show error: %s", s_error_names[_error_idx]);
            _blinks_remaining = s_blink_count[_error_idx];
            return true;
        }
    } while (_error_idx != org);

    return false;
}