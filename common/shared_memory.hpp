/**
 * shared_memory.hpp    Shared Memory IPC
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_SHARED_MEMORY_HPP
#define TRACK_SHARED_MEMORY_HPP

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// open queue, return pointer to shared mem
template <typename QueueType>
QueueType* open_shared_queue(const char* shm_name, bool is_writer) {
    int fd = shm_open(shm_name, O_RDWR | (is_writer ? O_CREAT : 0), 0666);
    if (fd == -1) return nullptr;

    if(is_writer && ftruncate(fd, sizeof(QueueType)) == -1) {
        ::close(fd);
        return nullptr;
    }

    void* ptr = mmap(nullptr, sizeof(QueueType), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if (ptr == MAP_FAILED) return nullptr;

    if(is_writer) new (ptr) QueueType(); // placement new to construct the queue in shared memory

    return static_cast<QueueType*>(ptr);
}

// unmap and close shared mem queue
template <typename QueueType>
void close_shared_queue(QueueType* queue, const char* shm_name, bool is_writer) {
    munmap(queue, sizeof(QueueType));
    if(is_writer) shm_unlink(shm_name);
}


#endif
