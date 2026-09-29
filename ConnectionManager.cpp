#include "ConnectionManager.h"
#include <WiFi.h>
#include "Config.h"
#include "util.h"

const char* const ConnectionManager::s_state_labels[] = {
    [ConnectionManager::STATE_DISCONNECTED] = "STATE_DISCONNECTED",
    [ConnectionManager::STATE_CONNECTING] = "STATE_CONNECTING",
    [ConnectionManager::STATE_CONNECTED] = "STATE_CONNECTED",
};

ConnectionManager::ConnectionManager()
{
    _status = -1;
    _state = STATE_DISCONNECTED;
}

void ConnectionManager::begin()
{   
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    _state_entry_time = millis();
    set_state(STATE_CONNECTING);
}

void ConnectionManager::loop()
{
    wl_status_t status = WiFi.status();
    if (status != _status) {
        log(LOG_INFO, "wifi: status code %d", status);
        _status = status;
    }

    unsigned long now = millis();
    switch (_state) {
        default:
        case STATE_DISCONNECTED:
            if (!WiFi.isConnected()) {
                set_state(STATE_CONNECTING);
            }
            break;

        case STATE_CONNECTING:
            if (now - _state_entry_time > 10000) {
                log(LOG_WARN, "wifi: connect timeout");
                set_state(STATE_DISCONNECTED);
            } else if (_status == WL_CONNECTED) {
                set_state(STATE_CONNECTED);
            }
            break;

        case STATE_CONNECTED:
            if (_status != WL_CONNECTED) {
                set_state(STATE_DISCONNECTED);
            }
            break;
    }
}

void ConnectionManager::set_connected_handler(conman_on_connected_f callback, void *ctx)
{
    _callback = callback;
    _usr_ctx = ctx;
}

void ConnectionManager::set_state(State state)
{
    if (state != _state) {
        log(LOG_INFO, "wifi: state changed from %s to %s", s_state_labels[_state], s_state_labels[state]);
    }

    _state = state;
    _state_entry_time = millis();

    switch (_state) {
        case STATE_DISCONNECTED:
            WiFi.disconnectAsync(true, true);
            update_connected_state(false);
            break;

        case STATE_CONNECTING:
            WiFi.begin(g_target_ssid, g_target_key);
            update_connected_state(false);
            break;

        case STATE_CONNECTED:
            update_connected_state(true);
            break;
            
        default:
            break;
    }
}

void ConnectionManager::update_connected_state(bool is_connected)
{
    if (is_connected == _is_connected) return;

    _is_connected = is_connected;
    fire_callback(is_connected);
}

void ConnectionManager::update_wifi_status(int status)
{
    if (status == _status) return;

    log(LOG_INFO, "wifi: status code %d", status);
    _status = status;
}

void ConnectionManager::fire_callback(bool is_connected)
{
    log(LOG_INFO, "wifi: %s", is_connected ? "connected" : "disconnected");
    if (_callback) _callback(_usr_ctx, is_connected);

    WiFiClient client();
}

void ConnectionManager::broadcast(const char* frame, int port)
{
    if (!_is_connected) return;
    size_t count = _udp.broadcastTo(frame, port);
}

