#ifndef INCLUDED_HMI_H_
#define INCLUDED_HMI_H_

#include <stdint.h>

enum hmi_error_e
{
    NO_WIFI_CONNECTION,
    NO_SM_CONNECTION,
    NUMBER_OF_ERRORS
};


class Hmi
{

    static constexpr unsigned no_error_on_ms = 5000;
    static constexpr unsigned intra_delay_ms = 1000;
    static constexpr unsigned on_time_ms = 100;
    static constexpr unsigned off_time_ms = 200;

    enum State
    {
        STATE_INTRA_PATTERN_DELAY,
        STATE_PATTERN_ON,
        STATE_PATTERN_OFF,
        STATE_NO_ERROR_ENTER,
        STATE_NO_ERROR_OFF,
    };

    static const char* const s_error_names[];
    static const char* const s_state_names[];
    static const int s_blink_count[];
    uint8_t _pin;
    State _state;
    unsigned long _state_entry_time;
    unsigned _active_errors;
    uint8_t _error_idx;
    uint8_t _blinks_remaining;

    public:
    explicit Hmi(uint8_t pin);

    void begin();
    void loop();

    void set_error(hmi_error_e error, bool is_error_active);

    private:
    bool select_error_to_show();
    void set_state(State state);
    void set_led(bool is_on);
};

#endif