#include <Arduino.h>
#include "Hmi.h"
#include "ConnectionManager.h"
#include "SmartMeter.h"
#include "util.h"
#include "ModbusTcpClient.h"
#include "ChargeController.h"
#include "NetworkLog.h"
#include "Config.h"

static size_t read_serial(void*, uint8_t* buf, size_t length);

static int s_frame_id = 0;
static Hmi s_hmi(LED_BUILTIN);
static HardwareSerial s_uart(0);
static ConnectionManager s_network;
static SmartMeter s_meter(read_serial, nullptr);
static ModbusTcpClient s_charger_modbus(g_charger_ip, g_charger_port);
static ChargeController s_ev_charger(s_charger_modbus, g_fuse_limit_ma);
static NetworkLog s_net_logger;
static bool s_start_network = false;
static unsigned long s_start_time = 0;

static size_t read_serial(void*, uint8_t* buf, size_t length)
{
    return s_uart.read(buf, length);
}

static void emit_log_frame(void* ctx, const char* buf, size_t length)
{
    s_network.broadcast(buf, 20001);
}

static void on_wifi_connected(void* ctx, bool is_connected) 
{
    (void)ctx;
    s_hmi.set_error(NO_WIFI_CONNECTION, !is_connected);
    s_net_logger.set_enable(is_connected);
}

static void on_meter_connected(void* ctx, bool is_connected) 
{
    (void)ctx;
    s_hmi.set_error(NO_SM_CONNECTION, !is_connected);
}

static void on_ev_charger_connected(void* ctx, bool is_connected) 
{
    (void)ctx;
    s_hmi.set_error(NO_CHARGER_CONNECTION, !is_connected);
}

static void on_smart_meter_frame(void* ctx, const sm_values_s* values)
{
    ++s_frame_id;

    char json[256];
    const char* tpl = "{\n" \
                      "  \"Device\": \"%s\",\n" \
                      "  \"SequenceNo\": %d,\n" \
                      "  \"PowerW\": [%d, %d, %d],\n" \
                      "  \"CurrentMa\": [%d, %d, %d],\n" \
                      "  \"PotentialMv\": [%d, %d, %d],\n" \
                      "  \"EnergyWh\": [%lld, %lld],\n" \
                      "  \"Timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\"\n"
                      "}\n";

    snprintf(json, sizeof(json), tpl,
             s_meter.name(),
             s_frame_id, 
             (int)values->power_w[0], (int)values->power_w[1], (int)values->power_w[2], 
             (int)values->current_ma[0], (int)values->current_ma[1], (int)values->current_ma[2], 
             (int)values->potential_mv[0], (int)values->potential_mv[1], (int)values->potential_mv[2], 
             (long long)values->import_wh, (long long)values->export_wh,
             values->timestamp.year,
             values->timestamp.month,
             values->timestamp.day,
             values->timestamp.hour,
             values->timestamp.min,
             values->timestamp.sec);

    log_meter_values(LOG_DEBUG, "app", values);
    s_network.broadcast(json);
    s_ev_charger.on_smart_meter_frame(values);
}

void setup()
{
    //setCpuFrequencyMhz(80);

    s_network.set_connected_handler(on_wifi_connected, nullptr);
    s_meter.set_connected_callback(on_meter_connected, nullptr);
    s_ev_charger.set_connected_callback(on_ev_charger_connected, nullptr);
    s_meter.set_frame_callback(on_smart_meter_frame, nullptr);
    s_net_logger.set_frame_callback(emit_log_frame, nullptr);

    if(ENABLE_LOGGING == LOG_MODE_SERIAL){
        log_set_output(&Serial);
        Serial.begin(115200);
    } else if(ENABLE_LOGGING == LOG_MODE_NETWORK) {
        log_set_output(&s_net_logger);
    }

    s_uart.begin(115200, SERIAL_8N1, D7, D6, true);
    //Serial1.begin(115200, SERIAL_8N1, D7, D6);
    s_hmi.begin();
    s_meter.begin();
    //s_network.begin();
    s_charger_modbus.begin();
    s_ev_charger.begin();

    s_start_time = millis();
}

void loop()
{
    if (s_start_network) {
        s_network.loop();
    }
    else if (millis() - s_start_time > 1000) {
        s_network.begin();
        s_start_network = true;
    }
    
    s_hmi.loop();
    s_meter.loop();
    s_charger_modbus.loop();
    s_ev_charger.loop();
    s_net_logger.loop();

    delay(5);
}

