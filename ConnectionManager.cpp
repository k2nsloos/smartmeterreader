#include "ConnectionManager.h"
#include <WiFi.h>
#include "Config.h"
#include "util.h"

ConnectionManager::ConnectionManager()
{
    
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
    if (_callback == nullptr) return;

    log("wifi: %s", is_connected ? "connected" : "disconnected");
    _callback(_usr_ctx, is_connected);
}