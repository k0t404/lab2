#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <unistd.h>


typedef enum {
    LOG_LEVEL_ERROR
} LogLevel;


void log_error(const char *msg);

ssize_t log_write(int fd, const char *msg, size_t len);
ssize_t log_write_str(int fd, const char *msg);
void log_write_int(int fd, int value);
void log_write_double(int fd, double value, int precision);

void log_printf(LogLevel level, const char *prefix, const char *msg);

#endif
