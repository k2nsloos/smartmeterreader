#ifndef INCLUDED_CONNECTION_MANAGER_H_
#define INCLUDED_CONNECTION_MANAGER_H_

#include <AsyncUDP.h>

typedef void (*conman_on_connected_f)(void*, bool);

class ConnectionManager
{
    AsyncUDP _udp;

    conman_on_connected_f _callback;
    void* _usr_ctx;
    bool _is_connected;
    int _status;

    public:
    ConnectionManager();

    void set_connected_handler(conman_on_connected_f callback, void *ctx);
    void begin();
    void loop();

    void broadcast(const char* frame);

    private:
    void fire_callback(bool is_connected);
};

#endif