#include <WiFi.h>

class ConnectionManager
{
    WiFiClass _wifi;
    WiFiClient _client;

    public:
    ConnectionManager();
    void begin();
    void loop();
    void scan();
};