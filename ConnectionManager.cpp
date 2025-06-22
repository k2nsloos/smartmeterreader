#include "ConnectionManager.h"
#include "util.h"

ConnectionManager::ConnectionManager()
{
    
}

void ConnectionManager::begin()
{   
}


void ConnectionManager::scan()
{
    int count = _wifi.scanNetworks();
    for (int network_id = 0; network_id < count; ++network_id)
    {
        char* ssid = _wifi.SSID(network_id);
        int enc = _wifi.encryptionType(network_id);
        int32_t rssi = _wifi.RSSI(network_id);

        log("%s %d %s\n", ssid, enc, rssi);
    }
}