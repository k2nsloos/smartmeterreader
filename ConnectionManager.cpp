#include "ConnectionManager.h"
#include <WiFi.h>
#include "Config.h"
#include "util.h"

ConnectionManager::ConnectionManager()
{
    _status = -1;
}

void ConnectionManager::begin()
{   
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(g_target_ssid, g_target_key);
}

void ConnectionManager::loop()
{
    wl_status_t status = WiFi.status();
    if (status != _status) {
        log("wifi: status code %d", status);
        _status = status;
    }

    bool is_connected = status == WL_CONNECTED;
    if (is_connected != _is_connected) {
        _is_connected = is_connected;
        fire_callback(is_connected);
    }
}

void ConnectionManager::set_connected_handler(conman_on_connected_f callback, void *ctx)
{
    _callback = callback;
    _usr_ctx = ctx;
}

void ConnectionManager::fire_callback(bool is_connected)
{
    log("wifi: %s", is_connected ? "connected" : "disconnected");
    if (_callback) _callback(_usr_ctx, is_connected);

    WiFiClient client();
}

void ConnectionManager::broadcast(const char* frame)
{
    if (!_is_connected) return;

    size_t count = _udp.broadcastTo(frame, 20000);
    log("wifi: broadcasted frame of %d bytes", count);
}