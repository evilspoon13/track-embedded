/**
 * can_socket.cpp       CAN Socket Interface
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "can_socket.hpp"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can/raw.h>
#include <cstring>
#include <unistd.h>

bool CanSocket::open(const std::string& interface) {
    fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if(fd_ == -1) return false;

    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if(ioctl(fd_, SIOCGIFINDEX, &ifr) == -1) {
        close();
        return false;
    }

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd_, (struct sockaddr* )&addr, sizeof(addr)) == -1) {
        close();
        return false;
    }

    return true;
}

bool CanSocket::read(can_frame& frame) {

    int nbytes = ::read(fd_, &frame, sizeof(struct can_frame));
    return nbytes == sizeof(frame);
}

void CanSocket::close() {
    ::close(fd_);
    fd_ = -1;
}
