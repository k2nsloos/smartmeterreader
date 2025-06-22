#include "SmartMeter.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

SmartMeter::SmartMeter(sm_read_f read, void* arg)
{
    _is_connected = false;
    _last_frame_time = 0;

    _read = read;
    _read_arg = arg;

    sm_parser_init(&_parser);
    sm_parser_set_frame_callback(&_parser, handle_smart_meter_frame, this);
}

void SmartMeter::begin()
{
    _is_connected = false;
    sm_parser_reset(&_parser);
}

void SmartMeter::loop()
{
    uint8_t buf[SMART_METER_READ_BUF_SIZE];
    size_t count = _read(_read_arg, buf, sizeof(buf));

    if (count == SM_READ_ERROR) {
        set_connected(false);
    } else {
        bool parse_ok = sm_parser_parse_str(&_parser, (char*)buf, count);
        if (!parse_ok) log("smart_meter: detected parse error");
    }

    check_timeout();
}

void SmartMeter::set_frame_callback(sm_frame_callback_f on_frame_parsed, void *arg)
{
    _on_frame_parsed = on_frame_parsed;
    _on_frame_parsed_arg = arg;
}

void SmartMeter::set_connected_callback(sm_connected_callback_f on_connected, void *arg)
{
    _on_connected = on_connected;
    _on_connected_arg = arg;
}

void SmartMeter::set_connected(bool is_connected)
{
    if (_is_connected == is_connected) return;
    _is_connected = is_connected;

    log("smart_meter: meter %s", is_connected ? "connected" : "disconnected");
    if (_on_connected) _on_connected(_on_connected_arg, is_connected);
}

void SmartMeter::emit_frame(const sm_values_s* values)
{
    if (_on_frame_parsed) _on_frame_parsed(_on_frame_parsed_arg, values);
}

void SmartMeter::check_timeout()
{
    if (!_is_connected) return;

    if (millis() - _last_frame_time >= 1000 * SMART_METER_TIMEOUT_SEC) {
        log("smart_meter: timeout no valid frame received in %d sec", (int)SMART_METER_TIMEOUT_SEC);
        set_connected(false);
    }
}

void SmartMeter::handle_smart_meter_frame(void *arg, const sm_values_s* values)
{
    SmartMeter* s = (SmartMeter*)arg;
    
    log("smart_meter: received frame, power: %ld W %ld W %ld W, energy: %ld Wh",
        (long)values->power_w[0],
        (long)values->power_w[1],
        (long)values->power_w[2],
        (long)(values->import_wh - values->export_wh));

    s->_last_frame_time = millis();
    s->set_connected(true);
    s->emit_frame(values);
}
