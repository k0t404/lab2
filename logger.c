#include "logger.h"

#include <stdlib.h>
#include <string.h>

#define LOG_BUFFER_SIZE 256


ssize_t log_write(int fd, const char *msg, size_t len) {
    if (!msg || len == 0)
        return 0;

    ssize_t total = 0;
    while ((size_t)total < len) {
        ssize_t written = write(fd, msg + total, len - total);
        if (written < 0)
            return -1;
        total += written;
    }
    return total;
}

ssize_t log_write_str(int fd, const char *msg) {
    if (!msg)
        return 0;
    return log_write(fd, msg, strlen(msg));
}


void log_write_int(int fd, int value)
{
    char buf[12];
    int pos = sizeof(buf) - 1;
    buf[pos] = '\0';

    unsigned int uval;
    if (value < 0) {
        uval = (unsigned int)(-(value + 1)) + 1;
    } else {
        uval = (unsigned int)value;
    }

    do {
        buf[--pos] = '0' + (uval % 10);
        uval /= 10;
    } while (uval > 0);

    if (value < 0)
        buf[--pos] = '-';

    log_write(fd, buf + pos, strlen(buf + pos));
}

void log_write_double(int fd, double value, int precision)
{
    if (value < 0) {
        log_write_str(fd, "-");
        value = -value;
    }

    long long int_part = (long long)value;
    log_write_int(fd, (int)int_part);

    if (precision <= 0)
        return;

    log_write_str(fd, ".");

    double frac = value - (double)int_part;
    for (int i = 0; i < precision; i++) {
        frac *= 10;
        int digit = (int)frac;
        if (digit < 0) digit = 0;
        if (digit > 9) digit = 9;
        char c = '0' + digit;
        log_write(fd, &c, 1);
        frac -= digit;
    }
}

void log_error(const char *msg)
{
    if (!msg)
        return;
    log_write_str(STDERR_FILENO, "error: ");
    log_write_str(STDERR_FILENO, msg);
    size_t len = strlen(msg);

    if (len == 0 || msg[len - 1] != '\n')
        log_write_str(STDERR_FILENO, "\n");
    exit(EXIT_FAILURE);
}

void log_printf(LogLevel level, const char *prefix, const char *msg)
{
    int fd = level == LOG_LEVEL_ERROR ? STDERR_FILENO : STDOUT_FILENO;

    if (prefix)
        log_write_str(fd, prefix);
    if (msg)
        log_write_str(fd, msg);
}
