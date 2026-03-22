/**
 * serial_port.cpp     UART Serial Port Wrapper
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "serial_port.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

// map integer baud rate to termios constant
static speed_t to_speed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B0;
    }
}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const char* device, int baud) {
    speed_t speed = to_speed(baud);
    if (speed == B0){
        std::fprintf(stderr, "unsuppotred baud rate: %d\n", baud);
        return false;
    }

    // O_RDONLY - we only read from GPS
    // O_NOCTTY - don't make this the controlling terminal
    fd_ = ::open(device, O_RDONLY | O_NOCTTY);
    if (fd_ < 0) {
        std::perror("serial open");
        return false;
    }

    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::perror("tcgetattr");
        close();
        return false;
    }

    // input/output baud rate
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // control flags: 8 data bits, no parity, 1 stop bit, enable receiver, local mode
    tty.c_cflag = speed | CS8 | CLOCAL | CREAD;

    // no input processing
    tty.c_iflag = 0;

    // raw output
    tty.c_oflag = 0;

    // no canonical mode, no echo, no signals
    tty.c_lflag = 0;

    // read behavior:
    // VMIN=1 - block until at least 1 byte available
    // VTIME=10 - 1 second timeout (units of 0.1s) after first byte
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;

    // flush any buffered data, then apply settings
    tcflush(fd_, TCIFLUSH);
    if(tcsetattr(fd_, TCSANOW, &tty) != 0){
        std::perror("tcsetattr");
        close();
        return false;
    }

    return true;
}

int SerialPort::read_line(char* buf, std::size_t max_len) {
    if (fd_ < 0 || max_len < 2)
        return -1;

    std::size_t pos = 0;
    while (pos < max_len - 1) {
        char c;
        ssize_t n = ::read(fd_, &c, 1);

        if (n < 0) {
            if (errno == EINTR) continue;  // interrupted by signal, retry
            return -1;
        }
        if (n == 0) break;// timeout / EOF

        buf[pos++] = c;
        if (c == '\n') break;
    }

    buf[pos] = '\0';
    return static_cast<int>(pos);
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
