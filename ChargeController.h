#ifndef INCLUDED_CHARGE_CONTROLLER_H_
#define INCLUDED_CHARGE_CONTROLLER_H_

#include "sm_common.h"
#include "ModbusTcpClient.h"

typedef void (*charge_controller_connected_callback_f)(void*, bool);

class ChargeController
{
    static const uint32_t s_poll_interval_ms = 3000;
    static const uint32_t s_smart_meter_timeout_ms = 10000;

    enum State {
        STATE_IDLE,
        STATE_READ_CHARGER_METER,
        STATE_SET_CHARGER_KEEP_ALIVE,
        STATE_SET_CHARGER_LIMIT
    };

    ModbusTcpClient& _client;
    uint32_t _fuse_limit_ma;

    bool _is_connected = false;
    bool _is_request_done = false;
    ModbusResult _modbus_rc;

    charge_controller_connected_callback_f _on_connected = NULL;
    void *_on_connected_arg;
    
    sm_values_s _smart_meter_meas;
    sm_values_s _charger_meter_meas;
    State _state = STATE_IDLE;
    uint32_t _state_entry_time = 0;
    uint32_t _last_poll_time = 0;
    uint32_t _last_smart_meter_frame_time;

    uint16_t _limit_ma;
    uint16_t _max_charging_current_buf = 0;
    uint16_t _charger_meter_buf[38];
    modbus_request_s _charger_meter_request;
    modbus_request_s _keep_alive_request;
    modbus_request_s _set_max_charging_current_request;

    public:
        ChargeController(ModbusTcpClient& client, uint32_t fuse_limit_ma);

        void begin();
        void loop();

        void set_connected_callback(charge_controller_connected_callback_f on_connected, void* arg);
        void on_smart_meter_frame(const sm_values_s* measurement);

    private:
        static void handle_modbus_request_done(void* arg, const modbus_request_s* request, uint8_t exception_code);
        void set_state(State state);
        void set_is_connected(bool is_connected);
        void set_charge_limit(int32_t limit_ma);
        int32_t determine_current_limit();
        
};

#endif