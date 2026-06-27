#ifndef INCLUDED_CONFIG_H_
#define INCLUDED_CONFIG_H_

enum {
    LOG_MODE_DISABLED,
    LOG_MODE_SERIAL,
    LOG_MODE_NETWORK
};

#define ENABLE_LOGGING LOG_MODE_SERIAL
#define LOG_LEVEL LOG_INFO

extern const unsigned g_fuse_limit_ma;

extern const char* g_charger_ip;
extern const int g_charger_port;

extern const char* g_target_ssid;
extern const char* g_target_key;

#endif;