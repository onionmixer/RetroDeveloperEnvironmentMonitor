/*
 * logger.c - Logging functionality implementation
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* Generate timestamp string for filename: YYYYMMDDHHmmss */
static void generate_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y%m%d%H%M%S", tm_info);
}

/* Generate timestamp string for log entries: YYYY-MM-DD HH:MM:SS.mmm */
static void generate_log_timestamp(char *buf, size_t size)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm *tm_info = localtime(&ts.tv_sec);
    int len = strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);

    /* Add milliseconds */
    if (len > 0 && (size_t)len < size - 5) {
        snprintf(buf + len, size - len, ".%03ld", ts.tv_nsec / 1000000);
    }
}

int logger_init(Logger *logger, bool enabled)
{
    logger->fp = NULL;
    logger->enabled = enabled;
    logger->filename[0] = '\0';

    if (!enabled) {
        return 0;
    }

    /* Generate filename with timestamp */
    char timestamp[32];
    generate_timestamp(timestamp, sizeof(timestamp));
    snprintf(logger->filename, sizeof(logger->filename),
             "emulator_log_%s.log", timestamp);

    /* Open log file */
    logger->fp = fopen(logger->filename, "w");
    if (logger->fp == NULL) {
        fprintf(stderr, "Failed to create log file: %s\n", logger->filename);
        logger->enabled = false;
        return -1;
    }

    /* Write header */
    char log_ts[64];
    generate_log_timestamp(log_ts, sizeof(log_ts));
    fprintf(logger->fp, "=== rdemonitor log started at %s ===\n", log_ts);
    fprintf(logger->fp, "=== Filename: %s ===\n\n", logger->filename);
    fflush(logger->fp);

    return 0;
}

void logger_write(Logger *logger, const char *raw_line)
{
    if (!logger->enabled || logger->fp == NULL || raw_line == NULL) {
        return;
    }

    char timestamp[64];
    generate_log_timestamp(timestamp, sizeof(timestamp));

    fprintf(logger->fp, "[%s] %s\n", timestamp, raw_line);
}

void logger_write_fmt(Logger *logger, const char *format, ...)
{
    if (!logger->enabled || logger->fp == NULL || format == NULL) {
        return;
    }

    char timestamp[64];
    generate_log_timestamp(timestamp, sizeof(timestamp));

    fprintf(logger->fp, "[%s] ", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(logger->fp, format, args);
    va_end(args);

    fprintf(logger->fp, "\n");
}

void logger_flush(Logger *logger)
{
    if (logger->fp != NULL) {
        fflush(logger->fp);
    }
}

void logger_close(Logger *logger)
{
    if (logger->fp != NULL) {
        char timestamp[64];
        generate_log_timestamp(timestamp, sizeof(timestamp));
        fprintf(logger->fp, "\n=== rdemonitor log ended at %s ===\n", timestamp);
        fclose(logger->fp);
        logger->fp = NULL;
    }
    logger->enabled = false;
}

bool logger_is_enabled(const Logger *logger)
{
    return logger->enabled && logger->fp != NULL;
}

const char *logger_get_filename(const Logger *logger)
{
    return logger->filename;
}
