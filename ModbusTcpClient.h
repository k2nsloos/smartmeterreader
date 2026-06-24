#ifndef INCLUDED_MODBUSTCP_CLIENT_H_
#define INCLUDED_MODBUSTCP_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

enum ModbusResult
{
    MODBUS_OK,
    MODBUS_BUSY,
    MODBUS_FATAL,
};

enum ModbusRegisterType
{
    MODBUS_REGISTER_TYPE_DISCRETE_INPUT,
    MODBUS_REGISTER_TYPE_COIL,
    MODBUS_REGISTER_TYPE_INPUT_REGISTER,
    MODBUS_REGISTER_TYPE_HOLDING_REGISTER,
};

enum ModbusExceptionCode
{
    MODBUS_EXCEPTION_NONE = 0,
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 1,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,
    MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE = 4,
    MODBUS_EXCEPTION_ACKNOWLEDGE = 5,
    MODBUS_EXCEPTION_SLAVE_IS_BUSY = 6,
    MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE = 10,

    MODBUS_EXCEPTION_INVALID_CRC = 251,
    MODBUS_EXCEPTION_PROTOCOL_VIOLATION = 252,
    MODBUS_EXCEPTION_NOT_CONNECTED = 253,
    MODBUS_EXCEPTION_CONNECTION_LOST = 254,
    MODBUS_EXCEPTION_TIMEOUT = 255,
};

typedef struct modbus_request_ { 
    uint8_t* buf;
    size_t count;
    ModbusRegisterType type;
    uint16_t register_no;
    uint8_t unit_id;
    bool is_write_request;
} modbus_request_s;

typedef struct modbus_tcp_header_ {
    uint16_t id;
    uint16_t reserved;
    uint16_t pdu_size;
} modbus_tcp_header_s;

typedef void (*request_completed_callback_f)(void* arg, const modbus_request_s* req, uint8_t exception_code);

class ModbusTcpClient
{
    static const int TcpHeaderSize = 6;
    static const int PduHeaderSize = 6;

    enum State {
        STATE_DISCONNECTED,
        STATE_CONNECTING,
        STATE_CONNECTED,
        STATE_WRITE_REQUEST,
        STATE_READ_RESPONSE_HEADER,
        STATE_READ_RESPONSE
    };

    const char* _ip;
    int _port;
    int _fd;

    uint32_t _frame_id;
    uint8_t _frame_buffer[272];
    size_t _frame_size;
    size_t _read_idx;
    size_t _write_idx;

    State _state;
    uint32_t _state_entry_time;

    const modbus_request_s* _active_request;
    request_completed_callback_f _request_callback;
    void* _request_callback_arg;

    public:
        ModbusTcpClient(const char* ip, int port);
        ~ModbusTcpClient();
        ModbusResult send_request(const modbus_request_s* req, request_completed_callback_f on_done, void* arg);
        void begin();
        void loop();

    private:
        static uint8_t get_function_code(const modbus_request_s* req);
        static size_t serialize_request(const modbus_request_s* req, uint32_t id, uint8_t* buf, size_t length);
        static modbus_tcp_header_s parse_tcp_header(const uint8_t* buf);
        void complete_request(uint8_t exception_code);
        void process_response();
        ModbusResult read_data();
        ModbusResult write_data();

        void set_state(State state);
        int tcp_open(const char* ip, int port);
        void tcp_close(int fd);
        ModbusResult tcp_connect(int fd);
        ModbusResult tcp_read(int fd, uint8_t* data, size_t length, size_t* act_length);
        ModbusResult tcp_write(int fd, const uint8_t* data, size_t length, size_t* act_length);     
};


#endif //INCLUDED_TCP_CLIENT_H_