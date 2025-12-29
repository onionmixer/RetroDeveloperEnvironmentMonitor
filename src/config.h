/*
 * config.h - Configuration management for rdemonitor
 *
 * Handles configuration file loading and command line argument parsing.
 * Priority: CLI arguments > ./rdemonitor.config > ~/.rdemonitor.config
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define CONFIG_MAX_ADDRESS_LEN 256
#define CONFIG_MAX_PATH_LEN    512

#define CONFIG_DEFAULT_ADDRESS "localhost"
#define CONFIG_DEFAULT_PORT    6502
#define CONFIG_DEFAULT_LOG_ALL false

#define CONFIG_FILENAME        "rdemonitor.config"

typedef struct {
    char debug_address[CONFIG_MAX_ADDRESS_LEN];
    int  debug_port;
    bool log_all;

    /* Internal flags to track what was set via CLI */
    bool cli_address_set;
    bool cli_port_set;
    bool cli_log_all_set;
} Config;

/*
 * Initialize config with default values
 */
void config_init(Config *cfg);

/*
 * Load configuration from file
 * Search order: ./rdemonitor.config -> ~/.rdemonitor.config
 * Returns: 0 on success, -1 if no config file found (uses defaults)
 */
int config_load(Config *cfg);

/*
 * Parse command line arguments
 * Format: --debug_address=VALUE --debug_port=VALUE --log_all=VALUE
 * Returns: 0 on success, -1 on error, 1 if --help requested
 */
int config_parse_args(Config *cfg, int argc, char **argv);

/*
 * Create default configuration file at ~/.rdemonitor.config
 * Returns: 0 on success, -1 on error
 */
int config_create_default(void);

/*
 * Print current configuration (for debugging)
 */
void config_print(const Config *cfg);

/*
 * Print usage information
 */
void config_print_usage(const char *program_name);

#endif /* CONFIG_H */
