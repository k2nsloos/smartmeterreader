#ifndef INCLUDED_CONNECTION_MANAGER_H_
#define INCLUDED_CONNECTION_MANAGER_H_

#include <AsyncUDP.h>

typedef void (*conman_on_connected_f)(void*, bool);


class ConnectionManager
{
    enum State {
        STATE_DISCONNECTED,
        STATE_CONNECTING,
        STATE_CONNECTED,
    };
    static const char* const s_state_labels[];

    AsyncUDP _udp;

    conman_on_connected_f _callback;
    void* _usr_ctx;
    bool _is_connected;
    State _state;
    unsigned long _state_entry_time;
    int _status;

    public:
    ConnectionManager();

    void set_connected_handler(conman_on_connected_f callback, void *ctx);
    void begin();
    void loop();

    void broadcast(const char* frame, int port=20000);

    private:
    void set_state(State state);
    void fire_callback(bool is_connected);
    void update_connected_state(bool is_connected);
    void update_wifi_status(int status);
};

#endif