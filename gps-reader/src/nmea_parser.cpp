/**
 * nmea_parser.cpp     NMEA 0183 Sentence Parser
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "nmea_parser.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>

static constexpr double KNOTS_TO_KMH = 1.852;

// XOR all bytes between '$' and '*', compare to hex digits after '*'
static bool verify_checksum(const char* sentence) {
    if (sentence[0] != '$') return false;

    unsigned char cs = 0;
    int i = 1;
    while (sentence[i] && sentence[i] != '*') {
        cs ^= static_cast<unsigned char>(sentence[i]);
        i++;
    }

    if (sentence[i] != '*') return false;

    unsigned long expected = std::strtoul(&sentence[i + 1], nullptr, 16);
    return cs == expected;
}

// convert NMEA DDDMM.MMMM + hemisphere to decimal degrees
static double parse_latlon(const char* field, char hemisphere) {
    double raw = std::strtod(field, nullptr);
    int degrees = static_cast<int>(raw / 100.0);
    double minutes = raw - degrees * 100.0;
    double result = degrees + minutes / 60.0;
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    return result;
}

// convert HHMMSS.SS + DDMMYY to epoch milliseconds
static int64_t parse_timestamp(const char* time_field, const char* date_field) {
    if (!time_field[0] || !date_field[0]) return 0;

    struct tm t{};
    t.tm_hour = (time_field[0] - '0') * 10 + (time_field[1] - '0');
    t.tm_min  = (time_field[2] - '0') * 10 + (time_field[3] - '0');
    t.tm_sec  = (time_field[4] - '0') * 10 + (time_field[5] - '0');

    t.tm_mday = (date_field[0] - '0') * 10 + (date_field[1] - '0');
    t.tm_mon  = (date_field[2] - '0') * 10 + (date_field[3] - '0') - 1;
    t.tm_year = (date_field[4] - '0') * 10 + (date_field[5] - '0') + 100; // 2000+

    time_t epoch = timegm(&t);
    if (epoch == -1) return 0;

    // add fractional seconds if present (e.g. "123519.00")
    int64_t ms = static_cast<int64_t>(epoch) * 1000;
    const char* dot = std::strchr(time_field, '.');
    if (dot) {
        double frac = std::strtod(dot, nullptr);
        ms += static_cast<int64_t>(frac * 1000.0);
    }

    return ms;
}

// split sentence into fields by comma, in-place. returns field count.
static int split_fields(char* buf, const char* fields[], int max_fields) {
    int count = 0;
    fields[count++] = buf;
    while (*buf && count < max_fields) {
        if (*buf == ',' || *buf == '*') {
            *buf = '\0';
            if (count < max_fields) fields[count++] = buf + 1;
        }
        buf++;
    }
    return count;
}

// get the sentence type starting after "$xx" (e.g. "RMC", "GGA")
static const char* sentence_type(const char* id) {
    size_t len = std::strlen(id);
    if (len < 5) return "";
    return id + len - 3; // last 3 chars: "RMC", "GGA", etc.
}

bool parse_nmea(const char* to_read, GpsMessage& msg) {
    if (!to_read || to_read[0] != '$') return false;
    if (!verify_checksum(to_read)) return false;

    char buf[256];
    std::strncpy(buf, to_read, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // strip trailing newline/CR
    size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';

    const char* fields[20]{};
    int nfields = split_fields(buf, fields, 20);

    const char* type = sentence_type(fields[0]);

    // $GPRMC / $GNRMC - position, speed, heading, date/time
    if (std::strcmp(type, "RMC") == 0 && nfields >= 10) {
        // field 2: status A=active V=void
        msg.has_fix = (fields[2][0] == 'A');

        if (msg.has_fix) {
            if (fields[3][0]) msg.latitude  = parse_latlon(fields[3], fields[4][0]);
            if (fields[5][0]) msg.longitude = parse_latlon(fields[5], fields[6][0]);
            if (fields[7][0]) msg.speed_kmh = std::strtod(fields[7], nullptr) * KNOTS_TO_KMH;
            if (fields[8][0]) msg.heading   = std::strtod(fields[8], nullptr);
        }

        msg.timestamp_ms = parse_timestamp(fields[1], fields[9]);
        return true;
    }

    // $GPGGA / $GNGGA - position, fix quality
    if (std::strcmp(type, "GGA") == 0 && nfields >= 7) {
        int fix_quality = std::atoi(fields[6]);
        msg.has_fix = (fix_quality >= 1);

        if (msg.has_fix) {
            if (fields[2][0]) msg.latitude  = parse_latlon(fields[2], fields[3][0]);
            if (fields[4][0]) msg.longitude = parse_latlon(fields[4], fields[5][0]);
        }
        return true;
    }

    return false;
}
