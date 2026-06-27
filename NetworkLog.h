#ifndef INCLUDED_NETWORK_LOG_H_
#define INCLUDED_NETWORK_LOG_H_

#include <Arduino.h>

typedef void(*transmit_frame_f)(void* arg, const char* buf, size_t length);

class NetworkLog: public Print
{  
    char _buf[4096];
    size_t _w_idx;
    uint32_t _frame_start_time;

    bool _is_enabled;
    transmit_frame_f _frame_cb;
    void* _frame_cb_arg;

    public:
    NetworkLog();
    void set_enable(bool is_enabled);
    void set_frame_callback(transmit_frame_f cb, void *arg);
    void loop();

    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    private:
    void emit();
};

#endif