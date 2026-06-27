#include "NetworkLog.h"
#include "util.h"

NetworkLog::NetworkLog()
  : _w_idx(0),
    _is_enabled(false),
    _frame_cb(NULL)
{
    
}

void NetworkLog::set_enable(bool is_enabled)
{
    _is_enabled = is_enabled;
}

void NetworkLog::set_frame_callback(transmit_frame_f cb, void *arg)
{
    _frame_cb = cb;
    _frame_cb_arg = arg;
}

void NetworkLog::loop()
{
    if (!_is_enabled || _w_idx == 0) return;

    uint32_t t = millis();
    if (t - _frame_start_time > 50) emit();
}

size_t NetworkLog::write(uint8_t ch)
{
    _buf[_w_idx++] = ch;
    if (_w_idx >= sizeof(_buf)) emit();
    return 1;
}

size_t NetworkLog::write(const uint8_t *buffer, size_t size)
{
    const uint8_t* end = buffer + size;

    while (buffer < end) {
        if (_w_idx == 0) _frame_start_time = millis();

        size_t src_rem = end - buffer;
        size_t dst_rem = sizeof(_buf) - _w_idx;
        size_t cpy_len = MIN(src_rem, dst_rem);
        memcpy(&_buf[_w_idx], buffer, cpy_len);

        buffer += cpy_len;
        _w_idx += cpy_len;
        if (_w_idx == sizeof(_buf)) emit();
    }

    return size;
}

void NetworkLog::emit()
{
    if (_is_enabled && _frame_cb) {
        _frame_cb(_frame_cb_arg, _buf, _w_idx);
    }

    _w_idx = 0;
}