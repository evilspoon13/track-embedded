#include "log_writer.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>

static constexpr const char *LOG_DIR = "/tmp/track-logs";

static std::string make_log_path() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%S.bin", &tm);
  return std::string(LOG_DIR) + "/" + buf;
}

LogWriter::LogWriter() {
  mkdir(LOG_DIR, 0755);
  path_ = make_log_path();
  file_ = fopen(path_.c_str(), "wb");
  if (!file_) {
    std::perror("Failed to open log file");
  } else {
    printf("Logging to %s\n", path_.c_str());
  }
}

LogWriter::~LogWriter() {
  if (file_) {
    fflush(file_);
    fclose(file_);
    file_ = nullptr;

    // write marker so cloud-bridge knows this file is finalized
    std::string done_path = path_.substr(0, path_.size() - 4) + ".done";
    FILE* marker = fopen(done_path.c_str(), "w");
    if (marker) fclose(marker);
  }
}

bool LogWriter::is_open() const { return file_ != nullptr; }

void LogWriter::flush() {
  if (file_)
    fflush(file_);
}

void LogWriter::write(uint32_t can_id, double value) {
  if (!file_)
    return;
  auto now = std::chrono::system_clock::now();
  LogEntry entry;
  entry.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
  entry.can_id = can_id;
  entry._pad = 0;
  entry.value = value;
  fwrite(&entry, sizeof(entry), 1, file_);
}
