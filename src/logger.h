/*
 * logger.h - Logging functionality for rdemonitor
 *
 * When log_all is enabled, logs all received data to a timestamped file.
 * Filename format: emulator_log_YYYYMMDDHHmmss.log
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdio.h>

#define LOGGER_MAX_FILENAME_LEN 256

typedef struct {
    FILE *fp;
    bool  enabled;
    char  filename[LOGGER_MAX_FILENAME_LEN];
} Logger;

/*
 * Initialize the logger
 * If enabled is true, creates a new log file with timestamp
 * Returns: 0 on success, -1 on error
 */
int logger_init(Logger *logger, bool enabled);

/*
 * Write a raw line to the log file
 * Adds timestamp prefix to each line
 */
void logger_write(Logger *logger, const char *raw_line);

/*
 * Write a formatted message to the log file
 */
void logger_write_fmt(Logger *logger, const char *format, ...);

/*
 * Flush the log file
 */
void logger_flush(Logger *logger);

/*
 * Close the logger and release resources
 */
void logger_close(Logger *logger);

/*
 * Check if logger is enabled and active
 */
bool logger_is_enabled(const Logger *logger);

/*
 * Get the log filename
 */
const char *logger_get_filename(const Logger *logger);

#endif /* LOGGER_H */
