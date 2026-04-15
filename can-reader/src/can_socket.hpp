/**
 * can_socket.hpp       CAN Socket Interface
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_CAN_SOCKET_HPP
#define TRACK_CAN_SOCKET_HPP

#include <linux/can.h>
#include <string>

class CanSocket {
public:

    CanSocket() = default;

    ~CanSocket() {
        if (fd_ != -1) {
            close();
        }
    }

    CanSocket(const CanSocket&) = delete;

    CanSocket& operator=(const CanSocket&) = delete;

    bool open(const std::string& interface);

    bool read(can_frame& frame);

    bool write(const can_frame& frame);

    bool set_read_timeout(int ms);

    void close();

private:
    int fd_ = -1;
};

#endif
