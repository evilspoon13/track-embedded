/**
 * serial_port.hpp     UART Serial Port Wrapper
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_SERIAL_PORT_HPP
#define TRACK_SERIAL_PORT_HPP

#include <cstddef>

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // open device (e.g. "/dev/serial0") at given baud rate, 8N1 raw mode
    bool open(const char* device, int baud);

    // read one line (up to '\n'), null-terminates the buffer
    // returns number of bytes read, or -1 on error
    int read_line(char* buf, std::size_t max_len);

    void close();

private:
    int fd_ = -1;
};

#endif
