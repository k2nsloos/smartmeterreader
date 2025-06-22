#ifndef INCLUDED_SMART_METER_H_
#define INCLUDED_SMART_METER_H_
#include "sm_common.h"
#include "sm_parser.h"

#define SMART_METER_TIMEOUT_SEC 15

typedef void (*sm_connected_callback_f)(void*, bool is_connected);
typedef size_t (*sm_read_f)(void*, uint8_t*, size_t);

class SmartMeter
{
    sm_parser_t _parser;

    sm_read_f _read;
    sm_frame_callback_f _on_frame_parsed;
    sm_connected_callback_f _on_connected;
    void* _read_arg;
    void* _on_frame_parsed_arg;
    void* _on_connected_arg;

    unsigned long _last_frame_time;
    bool _is_connected;

    public:
    SmartMeter(sm_read_f read, void* arg);

    void set_frame_callback(sm_frame_callback_f on_frame_parsed, void* arg);
    void set_connected_callback(sm_connected_callback_f on_connected, void* arg);
    void begin();
    void loop();

    const sm_values_s* values() const;

    private:
    static void handle_smart_meter_frame(void *arg, const sm_values_s* values);
    
    void set_connected(bool is_connected);
    void emit_frame(const sm_values_s* values);
    void check_timeout();
};

inline const sm_values_s* SmartMeter::values() const {
    return &_parser.last_frame;
}

#endif