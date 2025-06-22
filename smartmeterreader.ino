#include <Arduino.h>
#include "Hmi.h"
#include "ConnectionManager.h"
#include "SmartMeter.h"
#include "util.h"


static int s_frame_id = 0;
static Hmi s_hmi(LED_BUILTIN);
static ConnectionManager s_network;
static SmartMeter s_meter(read_serial, nullptr);
static HardwareSerial s_uart(0);


static size_t read_serial(void*, uint8_t* buf, size_t length)
{
    return s_uart.read(buf, length);
}

static void on_wifi_connected(void* ctx, bool is_connected) 
{
    (void)ctx;
    s_hmi.set_error(NO_WIFI_CONNECTION, !is_connected);
}

static void on_meter_connected(void* ctx, bool is_connected) 
{
    (void)ctx;
    s_hmi.set_error(NO_SM_CONNECTION, !is_connected);
}

static void on_smart_meter_frame(void* ctx, const sm_values_s* values)
{
    ++s_frame_id;

    char json[256];
    const char* tpl = "{\n" \
                      "  \"SequenceNo\": %d,\n" \
                      "  \"PowerW\": [%d, %d, %d],\n" \
                      "  \"EnergyWh\": [%lld, %lld],\n" \
                      "  \"Timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\"\n"
                      "}";

    snprintf(json, sizeof(json), tpl,
             s_frame_id,
             (int)values->power_w[0], (int)values->power_w[1], (int)values->power_w[2], 
             (long long)values->import_wh, (long long)values->export_wh,
             values->timestamp.year,
             values->timestamp.month,
             values->timestamp.day,
             values->timestamp.hour,
             values->timestamp.min,
             values->timestamp.sec);

    log("app: frame: %s", json);
    s_network.broadcast(json);
}

void setup()
{
    s_network.set_connected_handler(on_wifi_connected, nullptr);
    s_meter.set_connected_callback(on_meter_connected, nullptr);
    s_meter.set_frame_callback(on_smart_meter_frame, nullptr);

    Serial.begin(115200);
    s_uart.begin(115200);
    s_hmi.begin();
    s_meter.begin();
    s_network.begin();
}

void loop()
{
    s_hmi.loop();
    s_meter.loop();
    s_network.loop();
    delay(1);
}

