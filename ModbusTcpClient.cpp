#include "ModbusTcpClient.h"
#include <Arduino.h>
#include <Client.h>
#include <string.h>
#include "util.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>

enum ModbusFunctionCode {
    MODBUS_FUNC_INVALID = 0,
    MODBUS_FUNC_READ_COILS = 1,
    MODBUS_FUNC_READ_DISCRETE_INPUTS = 2,
    MODBUS_FUNC_READ_HOLDING_REGISTERS = 3,
    MODBUS_FUNC_READ_INPUT_REGISTERS = 4,
    MODBUS_FUNC_WRITE_SINGLE_COIL = 5,
    MODBUS_FUNC_WRITE_SINGLE_REGISTER = 6,
    MODBUS_FUNC_WRITE_MULTIPLE_COILS = 15,
    MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS = 16,
    MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS = 23,
};


static const char* s_state_labels[] = {
    "STATE_DISCONNECTED",
    "STATE_CONNECTING",
    "STATE_CONNECTED",
    "STATE_WRITE_REQUEST",
    "STATE_READ_RESPONSE_HEADER",
    "STATE_READ_RESPONSE"
};


ModbusTcpClient::ModbusTcpClient(const char* ip, int port)
{
    _ip = ip;
    _port = port;
}

ModbusTcpClient::~ModbusTcpClient()
{
    tcp_close(_fd);
    _fd = -1;
}

uint8_t ModbusTcpClient::get_function_code(const modbus_request_s* req)
{

    switch (req->type) {
        case MODBUS_REGISTER_TYPE_DISCRETE_INPUT:
            return req->is_write_request ? MODBUS_FUNC_INVALID : MODBUS_FUNC_READ_DISCRETE_INPUTS;

        case MODBUS_REGISTER_TYPE_COIL:
            return req->is_write_request ? MODBUS_FUNC_WRITE_MULTIPLE_COILS : MODBUS_FUNC_READ_COILS;

        case MODBUS_REGISTER_TYPE_INPUT_REGISTER:
            return req->is_write_request ? MODBUS_FUNC_INVALID : MODBUS_FUNC_READ_INPUT_REGISTERS;

        case MODBUS_REGISTER_TYPE_HOLDING_REGISTER:
            return req->is_write_request ? MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS : MODBUS_FUNC_READ_HOLDING_REGISTERS;
    }
}


size_t ModbusTcpClient::serialize_request(const modbus_request_s* req, uint32_t id, uint8_t* buf, size_t length)
{
    uint8_t function_code = get_function_code(req);
    if (function_code == MODBUS_FUNC_INVALID) {
        log("modbus: no function code for req");
        return 0;
    }

    size_t payload_size = req->is_write_request ? 2 * req->count : 0;
    size_t pdu_size = PduHeaderSize + (payload_size ? (1 + payload_size) : 0);
    size_t packet_size = TcpHeaderSize + pdu_size;
    if (packet_size > length) {
        log("modbus: packet to large %lu", (unsigned long)packet_size);
        return 0;
    }

    size_t offset = 0;
    buf[offset++] = id >> 8;
    buf[offset++] = id;
    buf[offset++] = 0;
    buf[offset++] = 0;
    buf[offset++] = pdu_size >> 8;
    buf[offset++] = pdu_size;
    buf[offset++] = req->unit_id;
    buf[offset++] = function_code;
    buf[offset++] = req->register_no >> 8;
    buf[offset++] = req->register_no;
    buf[offset++] = req->count >> 8;
    buf[offset++] = req->count;

    if (payload_size) {
        buf[offset++] = payload_size;
        memcpy(&buf[offset], req->buf, payload_size);
        offset += payload_size;
    }

    return offset;   
}

ModbusResult ModbusTcpClient::send_request(const modbus_request_s* req, request_completed_callback_f on_done, void* arg)
{
    if (_state < STATE_CONNECTED) return MODBUS_FATAL;
    if (_state > STATE_CONNECTED) return MODBUS_BUSY;

    _active_request = req;
    _request_callback = on_done;
    _request_callback_arg = arg;
    set_state(STATE_WRITE_REQUEST);
    return MODBUS_OK;
}

ModbusResult ModbusTcpClient::read_data()
{
    size_t act_length = 0;
    ModbusResult rc = tcp_read(_fd, &_frame_buffer[_write_idx], _frame_size - _write_idx, &act_length);
    _write_idx += act_length;

    if (rc == MODBUS_FATAL) return rc;
    return _write_idx >= _frame_size ? MODBUS_OK : MODBUS_BUSY;
}

ModbusResult ModbusTcpClient::write_data()
{
    size_t act_length = 0;
    ModbusResult rc = tcp_write(_fd, &_frame_buffer[_read_idx], _write_idx - _read_idx, &act_length);
    _read_idx += act_length;

    if (rc == MODBUS_FATAL) return rc;
    return _read_idx >= _write_idx ? MODBUS_OK : MODBUS_BUSY;
}

modbus_tcp_header_s ModbusTcpClient::parse_tcp_header(const uint8_t* buf)
{
    return (modbus_tcp_header_s) {
        .id = (uint16_t)(buf[0] << 8) | buf[1],
        .reserved = (uint16_t)(buf[2] << 8) | buf[3],
        .pdu_size = (uint16_t)(buf[4] << 8) | buf[5],
    };
}

void ModbusTcpClient::process_response()
{
    //log_buf(_frame_buffer, _frame_size);

    uint8_t unit_id = _frame_buffer[_read_idx++];
    uint8_t tmp = _frame_buffer[_read_idx++];
    uint8_t function_code = tmp & 0x7F;
    uint8_t exp_function_code = get_function_code(_active_request);
    bool is_exception = function_code & 0x80;

    if (unit_id != _active_request->unit_id || exp_function_code != function_code) {
        complete_request(MODBUS_EXCEPTION_PROTOCOL_VIOLATION);
        return;
    }

    if (is_exception) {
        uint8_t exception_code = _frame_buffer[_read_idx++];
        complete_request(MODBUS_EXCEPTION_PROTOCOL_VIOLATION);
        return;
    }

    if (_active_request->is_write_request) {
        complete_request(MODBUS_EXCEPTION_NONE);
        return;
    }

    uint8_t payload_size = _frame_buffer[_read_idx++];
    size_t buf_size = 2 * _active_request->count;
    size_t cpy_size = MIN(payload_size, buf_size);
    memcpy(_active_request->buf, &_frame_buffer[_read_idx], cpy_size);
    memset(&_active_request->buf[cpy_size], 0, buf_size - cpy_size);
    complete_request(MODBUS_EXCEPTION_NONE);
}

void ModbusTcpClient::begin()
{
    _state_entry_time = millis();
}

void ModbusTcpClient::loop()
{
    uint32_t t = millis();
    size_t act_length = 0;
    ModbusResult rc;

    switch (_state) {
        case STATE_DISCONNECTED:
            if (t - _state_entry_time > 5000) {
                set_state(STATE_CONNECTING);
            }
            break;

        case STATE_CONNECTING:
            rc = tcp_connect(_fd);
            if (rc == MODBUS_FATAL) {
                set_state(STATE_DISCONNECTED);
            } else if (rc == MODBUS_OK) {
                set_state(STATE_CONNECTED);
            } else if (t - _state_entry_time >= 5000) {
                set_state(STATE_DISCONNECTED);
            }
            break;

        case STATE_CONNECTED:
            break;

        case STATE_WRITE_REQUEST:
            rc = write_data();
            if (rc == MODBUS_FATAL) {
                set_state(STATE_DISCONNECTED); 
            } else if (rc == MODBUS_OK) {
                set_state(STATE_READ_RESPONSE_HEADER);
            } else if (t - _state_entry_time > 1000) {
                complete_request(MODBUS_EXCEPTION_TIMEOUT);
                set_state(STATE_DISCONNECTED);
            }
            break;

        case STATE_READ_RESPONSE_HEADER:
            rc = read_data();

            if (rc == MODBUS_FATAL) {
                set_state(STATE_DISCONNECTED);
            } else if (rc == MODBUS_OK) {
                modbus_tcp_header_s hdr = parse_tcp_header(_frame_buffer);
                _read_idx = _frame_size;
                _frame_size += hdr.pdu_size;

                if (hdr.id != _frame_id || _frame_size >= sizeof(_frame_buffer)) {
                    complete_request(MODBUS_EXCEPTION_PROTOCOL_VIOLATION);
                    set_state(STATE_DISCONNECTED);
                } else {
                    set_state(STATE_READ_RESPONSE);
                }
            } else if (t - _state_entry_time > 1000) {
                complete_request(MODBUS_EXCEPTION_TIMEOUT);
                set_state(STATE_DISCONNECTED);
            }
            break;
        
        case STATE_READ_RESPONSE:
            rc = read_data();
            if (rc == MODBUS_FATAL) {
                set_state(STATE_DISCONNECTED);
            } else if (rc == MODBUS_OK) {
                process_response();
                set_state(STATE_CONNECTED);
            } else if (t - _state_entry_time > 1000) {
                complete_request(MODBUS_EXCEPTION_TIMEOUT);
                set_state(STATE_DISCONNECTED);
            }
            break;
    }
}

void ModbusTcpClient::set_state(State state)
{
    log("modbus: state changed from %s to %s", s_state_labels[_state], s_state_labels[state]);

    _state = state;
    _state_entry_time = millis();

    switch (_state)
    {
        case STATE_DISCONNECTED:
            tcp_close(_fd);
            _fd = -1;
            complete_request(MODBUS_EXCEPTION_NOT_CONNECTED);
            break;

        case STATE_CONNECTING:
            _fd = tcp_open(_ip, _port);
            break;

        case STATE_CONNECTED:
            break;

        case STATE_WRITE_REQUEST:
            _frame_size = serialize_request(_active_request, ++_frame_id, _frame_buffer, sizeof(_frame_buffer));
            //log_buf(_frame_buffer, _frame_size);
            Serial.flush();
            _write_idx = _frame_size;
            _read_idx = 0;
            break;

        case STATE_READ_RESPONSE_HEADER:
            _frame_size = TcpHeaderSize;
            _write_idx = 0;
            _read_idx = 0;
            break;

        case STATE_READ_RESPONSE:
            break;
    }
}

void ModbusTcpClient::complete_request(uint8_t exception_code)
{
    const modbus_request_s* req = _active_request;
    _active_request = NULL;

    if (!req) return;

    if (_request_callback) {
        _request_callback(_request_callback_arg, req, exception_code);
    }
}

int ModbusTcpClient::tcp_open(const char* ip_str, int port)
{
    struct in_addr ip_address;
    int rc = inet_pton(AF_INET, ip_str, &ip_address);
    if (rc != 1) return -1;

    struct sockaddr_storage serveraddr = { 0 };
    struct sockaddr_in *tmpaddr = (struct sockaddr_in *)&serveraddr;
    tmpaddr->sin_family = AF_INET;
    tmpaddr->sin_addr = ip_address;
    tmpaddr->sin_port = htons(port);
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        log("tcp: socket error: %d \"%s\"", fd, errno, strerror(errno));
        return -1;
    }

    int nodelay = 1;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    rc = connect(fd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in));
    if (rc < 0 && errno != EINPROGRESS) {
        log("tcp: error connecting on fd %d: %d \"%s\"", fd, errno, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

void ModbusTcpClient::tcp_close(int fd)
{
    if (fd < 0) return;
    close(fd);
}

ModbusResult ModbusTcpClient::tcp_connect(int fd)
{
    if (fd < 0) return MODBUS_FATAL;

    struct pollfd fdset = {
        .fd = _fd,
        .events = POLLOUT
    };

    struct timeval tv = { 0 };
    int rc = poll(&fdset, 1, 0);
    if (rc == 0) return MODBUS_BUSY;
    if (rc == 1) return MODBUS_OK;
    return MODBUS_FATAL;
}

static bool linux_is_try_again_error(int error)
{
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
}

static ModbusResult linux_map_rc(int error, ssize_t bytes, size_t* act_len, bool is_read)
{
    *act_len = 0;

    // OK
    if (bytes > 0) {
        *act_len = bytes;
        return MODBUS_OK;
    }

    // EOF
    if (bytes == 0) return is_read ? MODBUS_FATAL : MODBUS_OK;

    // Failure
    bool is_try_again = linux_is_try_again_error(error);
    return is_try_again ? MODBUS_BUSY : MODBUS_FATAL;
}

ModbusResult ModbusTcpClient::tcp_read(int fd, uint8_t* data, size_t length, size_t* act_length)
{
    if (!length) return MODBUS_OK;
    ssize_t bytes = read(_fd, data, length);
    return linux_map_rc(errno, bytes, act_length, true);
}

ModbusResult ModbusTcpClient::tcp_write(int fd, const uint8_t* data, size_t length, size_t* act_length)
{
    if (!length) return MODBUS_OK;
    ssize_t bytes = write(_fd, data, length);
    return linux_map_rc(errno, bytes, act_length, false);
}
