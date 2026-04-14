/**
 * status_shm.hpp       Process Status Shared Memory
 *
 * Small POSIX shm segment holding latest status flags from producer
 * processes. Read by gpio-reader for LED
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_STATUS_SHM_HPP
#define TRACK_STATUS_SHM_HPP

#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr const char* STATUS_SHM = "/track_status";

struct TrackStatus {
    std::atomic<uint8_t> logger_recording;
    std::atomic<uint8_t> cloud_ws_connected;
    std::atomic<uint8_t> can_healthy;
};

inline TrackStatus* open_status_shm() {
    int fd = shm_open(STATUS_SHM, O_RDWR | O_CREAT, 0666);
    if (fd == -1) return nullptr;

    if (ftruncate(fd, sizeof(TrackStatus)) == -1){
        ::close(fd);
        return nullptr;
    }

    void* ptr = mmap(nullptr, sizeof(TrackStatus), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (ptr == MAP_FAILED)
        return nullptr;

    return static_cast<TrackStatus*>(ptr);
}

inline void close_status_shm(TrackStatus* status) {
    if (status)
        munmap(status, sizeof(TrackStatus));
}

#endif
