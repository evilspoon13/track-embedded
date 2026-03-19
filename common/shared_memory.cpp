/**
 * shared_memory.cpp    Shared Memory IPC
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "shared_memory.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

TelemetryQueue* open_shared_queue(bool is_writer) {
    int fd = shm_open(SHM_NAME, O_RDWR | (is_writer ? O_CREAT : 0), 0666);
    if (fd == -1) return nullptr;

    if(is_writer && ftruncate(fd, sizeof(TelemetryQueue)) == -1) {
        ::close(fd);
        return nullptr;
    }

    void* ptr = mmap(nullptr, sizeof(TelemetryQueue), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if (ptr == MAP_FAILED) return nullptr;

    if(is_writer) new (ptr) TelemetryQueue(); // placement new to construct the queue in shared memory

    return static_cast<TelemetryQueue*>(ptr);
}

void close_shared_queue(TelemetryQueue* queue, bool is_writer) {
    munmap(queue, sizeof(TelemetryQueue));
    if(is_writer) shm_unlink(SHM_NAME);
}
