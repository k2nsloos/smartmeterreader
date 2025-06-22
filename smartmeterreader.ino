#include <Arduino.h>
#include "Hmi.h"
#include "ConnectionManager.h"

static Hmi s_hmi(LED_BUILTIN);
static ConnectionManager s_network;


static void on_wifi_connected(void* ctx, bool is_connected) {
    (void)ctx;
    
    s_hmi.set_error(NO_WIFI_CONNECTION, !is_connected);
}

void setup()
{
    Serial.begin(115200);
    s_hmi.begin();
    s_network.begin();
}

void loop()
{
    s_hmi.loop();
    s_network.loop();
}

