/**
 * log_writer.hpp       Telemetry Log Writer
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_LOG_WRITER_HPP
#define TRACK_LOG_WRITER_HPP

#include <cstdint>
#include <cstdio>

// Binary log entry layout (24 bytes):
// | timestamp_ms (int64) | can_id (uint32) | _pad (uint32) | value (double) |
struct LogEntry {
    int64_t  timestamp_ms;
    uint32_t can_id;
    uint32_t _pad;
    double   value;
};

class LogWriter {
public:
    LogWriter();
    ~LogWriter();

    LogWriter(const LogWriter&) = delete;
    LogWriter& operator=(const LogWriter&) = delete;

    bool is_open() const;
    void write(uint32_t can_id, double value);
    void flush();

private:
    FILE* file_ = nullptr;
};

#endif
