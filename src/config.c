/*
 * config.c - Configuration management implementation
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

/* Helper function to get home directory path */
static const char *get_home_dir(void)
{
    const char *home = getenv("HOME");
    if (home == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            home = pw->pw_dir;
        }
    }
    return home;
}

/* Helper function to trim whitespace */
static char *trim(char *str)
{
    char *end;

    /* Trim leading space */
    while (*str == ' ' || *str == '\t') str++;

    if (*str == 0)
        return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        end--;

    end[1] = '\0';
    return str;
}

/* Parse a single config line */
static void parse_config_line(Config *cfg, const char *line)
{
    char key[64];
    char value[256];
    const char *eq;

    /* Skip comments and empty lines */
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
        return;

    eq = strchr(line, '=');
    if (eq == NULL)
        return;

    /* Extract key */
    size_t key_len = eq - line;
    if (key_len >= sizeof(key))
        key_len = sizeof(key) - 1;
    strncpy(key, line, key_len);
    key[key_len] = '\0';

    /* Extract value */
    strncpy(value, eq + 1, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';

    /* Trim both */
    char *trimmed_key = trim(key);
    char *trimmed_value = trim(value);

    /* Apply to config (only if not already set via CLI) */
    if (strcmp(trimmed_key, "debug_address") == 0 && !cfg->cli_address_set) {
        strncpy(cfg->debug_address, trimmed_value, CONFIG_MAX_ADDRESS_LEN - 1);
        cfg->debug_address[CONFIG_MAX_ADDRESS_LEN - 1] = '\0';
    }
    else if (strcmp(trimmed_key, "debug_port") == 0 && !cfg->cli_port_set) {
        cfg->debug_port = atoi(trimmed_value);
    }
    else if (strcmp(trimmed_key, "log_all") == 0 && !cfg->cli_log_all_set) {
        cfg->log_all = (strcmp(trimmed_value, "true") == 0 ||
                        strcmp(trimmed_value, "1") == 0 ||
                        strcmp(trimmed_value, "yes") == 0);
    }
}

void config_init(Config *cfg)
{
    strncpy(cfg->debug_address, CONFIG_DEFAULT_ADDRESS, CONFIG_MAX_ADDRESS_LEN - 1);
    cfg->debug_address[CONFIG_MAX_ADDRESS_LEN - 1] = '\0';
    cfg->debug_port = CONFIG_DEFAULT_PORT;
    cfg->log_all = CONFIG_DEFAULT_LOG_ALL;

    cfg->cli_address_set = false;
    cfg->cli_port_set = false;
    cfg->cli_log_all_set = false;
}

int config_load(Config *cfg)
{
    char path[CONFIG_MAX_PATH_LEN];
    FILE *fp = NULL;

    /* Try ./rdemonitor.config first */
    snprintf(path, sizeof(path), "./%s", CONFIG_FILENAME);
    fp = fopen(path, "r");

    /* Try ~/.rdemonitor.config if local not found */
    if (fp == NULL) {
        const char *home = get_home_dir();
        if (home != NULL) {
            snprintf(path, sizeof(path), "%s/.%s", home, CONFIG_FILENAME);
            fp = fopen(path, "r");
        }
    }

    if (fp == NULL) {
        /* No config file found - will use defaults */
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        parse_config_line(cfg, line);
    }

    fclose(fp);
    return 0;
}

int config_parse_args(Config *cfg, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return 1;
        }

        if (strncmp(argv[i], "--debug_address=", 16) == 0) {
            strncpy(cfg->debug_address, argv[i] + 16, CONFIG_MAX_ADDRESS_LEN - 1);
            cfg->debug_address[CONFIG_MAX_ADDRESS_LEN - 1] = '\0';
            cfg->cli_address_set = true;
        }
        else if (strncmp(argv[i], "--debug_port=", 13) == 0) {
            cfg->debug_port = atoi(argv[i] + 13);
            cfg->cli_port_set = true;
        }
        else if (strncmp(argv[i], "--log_all=", 10) == 0) {
            const char *val = argv[i] + 10;
            cfg->log_all = (strcmp(val, "true") == 0 ||
                           strcmp(val, "1") == 0 ||
                           strcmp(val, "yes") == 0);
            cfg->cli_log_all_set = true;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

int config_create_default(void)
{
    char path[CONFIG_MAX_PATH_LEN];
    const char *home = get_home_dir();

    if (home == NULL) {
        fprintf(stderr, "Cannot determine home directory\n");
        return -1;
    }

    snprintf(path, sizeof(path), "%s/.%s", home, CONFIG_FILENAME);

    /* Check if file already exists */
    if (access(path, F_OK) == 0) {
        /* File exists, don't overwrite */
        return 0;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Cannot create config file: %s\n", path);
        return -1;
    }

    fprintf(fp, "# rdemonitor configuration file\n");
    fprintf(fp, "# Created automatically\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Debug server address\n");
    fprintf(fp, "debug_address=%s\n", CONFIG_DEFAULT_ADDRESS);
    fprintf(fp, "\n");
    fprintf(fp, "# Debug server port\n");
    fprintf(fp, "debug_port=%d\n", CONFIG_DEFAULT_PORT);
    fprintf(fp, "\n");
    fprintf(fp, "# Log all received data to file\n");
    fprintf(fp, "log_all=false\n");

    fclose(fp);
    return 0;
}

void config_print(const Config *cfg)
{
    printf("Configuration:\n");
    printf("  debug_address: %s\n", cfg->debug_address);
    printf("  debug_port:    %d\n", cfg->debug_port);
    printf("  log_all:       %s\n", cfg->log_all ? "true" : "false");
}

void config_print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Retro Developer Environment Monitor\n");
    printf("\n");
    printf("Options:\n");
    printf("  --debug_address=ADDR  Debug server address (default: %s)\n", CONFIG_DEFAULT_ADDRESS);
    printf("  --debug_port=PORT     Debug server port (default: %d)\n", CONFIG_DEFAULT_PORT);
    printf("  --log_all=BOOL        Log all data to file (default: false)\n");
    printf("  --help, -h            Show this help message\n");
    printf("\n");
    printf("Configuration file:\n");
    printf("  Searches for config in order:\n");
    printf("    1. ./rdemonitor.config\n");
    printf("    2. ~/.rdemonitor.config\n");
    printf("\n");
    printf("  Command line arguments take priority over config file.\n");
}
