
#include "ChargeController.h"
#include "ModbusTcpClient.h"
#include "util.h"
#include <Arduino.h>

static const uint8_t s_keep_alive_value[] = {0x00, 0x01};

static const char* const s_state_labels[] = {
    "STATE_IDLE",
    "STATE_READ_CHARGER_METER",
    "STATE_SET_CHARGER_KEEP_ALIVE",
    "STATE_SET_CHARGER_LIMIT",
};


static uint16_t swap_b16(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t swap_b32(uint16_t v1, uint16_t v2)
{
    uint32_t b4 = (v1 & 0xFF) << 24;
    uint32_t b3 = (v1 >> 8) << 16;
    uint32_t b2 = (v2 & 0xFF) << 8;
    uint32_t b1 = (v2 >> 8) << 0;
    return b1 | b2 | b3 | b4;
}

ChargeController::ChargeController(ModbusTcpClient& client, uint32_t fuse_limit_ma)
    : _fuse_limit_ma(fuse_limit_ma),
      _client(client)
{
    _charger_meter_request = (modbus_request_s) {
        .buf = (uint8_t*)_charger_meter_buf,
        .count = ARRAY_SIZE(_charger_meter_buf),
        .type = MODBUS_REGISTER_TYPE_INPUT_REGISTER,
        .register_no = 1000,
        .unit_id = 1,
        .is_write_request = false,
    };

    _keep_alive_request = (modbus_request_s) {
        .buf = (uint8_t*)&s_keep_alive_value,
        .count = 1,
        .type = MODBUS_REGISTER_TYPE_HOLDING_REGISTER,
        .register_no = 6000,
        .unit_id = 1,
        .is_write_request = true,
    };

    _set_max_charging_current_request = (modbus_request_s) {
        .buf = (uint8_t*)&_max_charging_current_buf,
        .count = 1,
        .type = MODBUS_REGISTER_TYPE_HOLDING_REGISTER,
        .register_no = 5004,
        .unit_id = 1,
        .is_write_request = true
    };

    _limit_ma = -1;
}

void ChargeController::begin()
{
    _state_entry_time = millis();
    _last_smart_meter_frame_time = _state_entry_time - s_smart_meter_timeout_ms;
}

void ChargeController::loop()
{
    uint32_t t = millis();

    switch (_state) {
        case STATE_IDLE:
            if (t - _last_poll_time >= s_poll_interval_ms) {
                _last_poll_time = t;
                set_state(STATE_READ_CHARGER_METER);
            }
            break;

        case STATE_READ_CHARGER_METER:
            if (!_is_request_done) return;
            
            if (_modbus_rc == MODBUS_OK) {
                set_state(STATE_SET_CHARGER_KEEP_ALIVE);
            } else if (_modbus_rc == MODBUS_BUSY) {
                set_state(STATE_READ_CHARGER_METER); // try again
            } else {
                set_is_connected(false);
                set_state(STATE_IDLE);
            }

            break;

        case STATE_SET_CHARGER_KEEP_ALIVE:
            if (!_is_request_done) return;

            if (_modbus_rc == MODBUS_OK) {
                set_state(STATE_SET_CHARGER_LIMIT);
            } else if (_modbus_rc == MODBUS_BUSY) {
                set_state(STATE_SET_CHARGER_KEEP_ALIVE); // try again
            } else {
                set_is_connected(false);
                set_state(STATE_IDLE);
            }

            break;

        case STATE_SET_CHARGER_LIMIT:
            if (!_is_request_done) return;

            if (_modbus_rc == MODBUS_OK) {
                set_is_connected(true);
                set_state(STATE_IDLE);
            } else if (_modbus_rc == MODBUS_BUSY) {
                set_state(STATE_SET_CHARGER_LIMIT); // try again
            } else {
                set_is_connected(false);
                set_state(STATE_IDLE);
            }
            break;
    }
}

void ChargeController::set_connected_callback(charge_controller_connected_callback_f on_connected, void* arg)
{
    _on_connected = on_connected;
    _on_connected_arg = arg;
}

void ChargeController::on_smart_meter_frame(const sm_values_s* measurement)
{
    _smart_meter_meas = *measurement;
    _last_smart_meter_frame_time = millis();
}

void ChargeController::set_is_connected(bool is_connected)
{
    if (is_connected == _is_connected) return;

    log(LOG_INFO, "charger: %s", is_connected ? "connected" : "disconnected");
    _is_connected = is_connected;
    if (_on_connected) _on_connected(_on_connected_arg, is_connected);
}

void ChargeController::set_charge_limit(int32_t limit_ma)
{
    if (limit_ma == _limit_ma) return;

    _limit_ma = limit_ma;
    uint16_t limit_a = limit_ma / 1000;
    _max_charging_current_buf = swap_b16(limit_a);

    log(LOG_INFO, "charger: set charge limit to %d A", (int)limit_a);
}

void ChargeController::set_state(State state)
{
    log(LOG_DEBUG, "charger: state changed from %s to %s", s_state_labels[_state], s_state_labels[state]);

    _state = state;
    _state_entry_time = millis();

    switch (_state) {
        case STATE_IDLE:
            break;

        case STATE_READ_CHARGER_METER:
            _modbus_rc = _client.send_request(&_charger_meter_request, &handle_modbus_request_done, this);
            _is_request_done = _modbus_rc != MODBUS_OK;
            break;

        case STATE_SET_CHARGER_KEEP_ALIVE:
            _modbus_rc = _client.send_request(&_keep_alive_request, &handle_modbus_request_done, this);
            _is_request_done = _modbus_rc != MODBUS_OK;
            break;

        case STATE_SET_CHARGER_LIMIT:
            if (_state_entry_time -_last_smart_meter_frame_time <= s_poll_interval_ms) {
                int32_t limit_ma = determine_current_limit();
                set_charge_limit(limit_ma);
            } else if (_state_entry_time - _last_smart_meter_frame_time >= s_smart_meter_timeout_ms) {
                set_charge_limit(0);
            } else {
                // keep current value
            }

            _modbus_rc = _client.send_request(&_set_max_charging_current_request, &handle_modbus_request_done, this);
            _is_request_done = _modbus_rc != MODBUS_OK;
            break;
    }
}

void ChargeController::handle_modbus_request_done(void *arg, const modbus_request_s *request, uint8_t exception_code)
{
    ChargeController* c = static_cast<ChargeController*>(arg);
    uint32_t t = millis();
    bool is_success = exception_code == MODBUS_EXCEPTION_NONE;
    

    switch (c->_state) {
        case STATE_READ_CHARGER_METER:
            if (is_success) {
                c->_charger_meter_meas = (sm_values_s) {
                    .timestamp = { 0 },
                    .power_w = {
                        swap_b32(c->_charger_meter_buf[24], c->_charger_meter_buf[25]),
                        swap_b32(c->_charger_meter_buf[28], c->_charger_meter_buf[29]),
                        swap_b32(c->_charger_meter_buf[32], c->_charger_meter_buf[33]),
                    },
                    .potential_mv = {
                        1000 * swap_b16(c->_charger_meter_buf[14]), 
                        1000 * swap_b16(c->_charger_meter_buf[16]), 
                        1000 * swap_b16(c->_charger_meter_buf[18])
                    },

                    .current_ma = { 
                        swap_b16(c->_charger_meter_buf[8]), 
                        swap_b16(c->_charger_meter_buf[10]), 
                        swap_b16(c->_charger_meter_buf[12])
                    },

                    .import_wh = 100 * swap_b32(c->_charger_meter_buf[36], c->_charger_meter_buf[37]),
                    .export_wh = 0,                    
                };

                log_meter_values(LOG_DEBUG, "charger", &c->_charger_meter_meas);

            }
            c->_is_request_done = true;
            c->_modbus_rc = is_success ? MODBUS_OK : MODBUS_FATAL;
            break;

        case STATE_SET_CHARGER_KEEP_ALIVE:
        case STATE_SET_CHARGER_LIMIT:
            c->_is_request_done = true;
            c->_modbus_rc = is_success ? MODBUS_OK : MODBUS_FATAL;
            break;

        default:
            log(LOG_WARN, "charger: got modbus result in state %d", c->_state);
            break;
    }
}

/*
uint32_t ChargeController::determine_current_limit() {
    if (_last_smart_meter_frame_time >= s_smart_meter_timeout_ms) return 0;

    float charger_limit_a;
    float fuse_limit_a = (float)_fuse_limit_ma / 1000;
    float min_limit = fuse_limit_a;

    for (int idx = 0; idx < 3; ++idx) {
        float active_a = (float)1000 * _smart_meter_meas.power_w[idx] / _smart_meter_meas.potential_mv[idx];
        float mains_a =  (float)_smart_meter_meas.current_ma[idx] / 1000;
        float charger_a =  (float)_charger_meter_meas.current_ma[idx] / 1000;
        float tmp = mains_a * mains_a - active_a * active_a;
        float reactive_a = sqrtf(MAX(tmp, 0));
        float charger_limit_a = fuse_limit_a - reactive_a - active_a;

        log(LOG_DEBUG, "charger: phase %d, active_ma %ld, reactive_ma %ld, charger_ma %ld, limit_ma %d", 
            idx + 1, 
            (long)(1000 * active_a), 
            (long)(1000 * reactive_a), 
            (long)_smart_meter_meas.current_ma[idx],
            (long)(1000 * charger_limit_a));

        min_limit = MIN(charger_limit_a, min_limit);
    }

    if (min_limit < 0) min_limit = 0;
    return (long)(1000 * min_limit);
}
*/

int32_t ChargeController::determine_current_limit() {
    int32_t min_limit = _fuse_limit_ma;
    bool is_any_phase_active = false;
    bool phase_active[3] = {0};

    for (int idx = 0; idx < 3; ++idx) {
        
        if (_charger_meter_meas.current_ma[idx] > 1000) {
            phase_active[idx] = 1;
            is_any_phase_active = true;
        }
    }
    
    for (int idx = 0; idx < 3; ++idx) {
        int32_t charger_limit_ma = _fuse_limit_ma - _smart_meter_meas.current_ma[idx] + _charger_meter_meas.current_ma[idx];
        if (!is_any_phase_active || phase_active[idx]) {
            min_limit = MIN(charger_limit_ma, min_limit);
        }
    }

    if (min_limit < 0) min_limit = 0;
    return min_limit;
}