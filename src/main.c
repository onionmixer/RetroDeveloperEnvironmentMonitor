/*
 * main.c - rdemonitor entry point
 *
 * Retro Developer Environment Monitor
 * Monitors debug output from AppleWin and openMSX emulators.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

#include "config.h"
#include "network.h"
#include "parser.h"
#include "logger.h"
#include "data_store.h"
#include "ui.h"

/* Reconnection settings */
#define RECONNECT_INTERVAL_SEC  3
#define RECONNECT_MAX_ATTEMPTS  0  /* 0 = unlimited */
#define DISCONNECT_RESET_SEC    10 /* Reset data after 10 seconds of disconnect */

/* Global state */
static volatile int g_resize_pending = 0;
static Config g_config;
static NetworkContext g_network;
static Logger g_logger;
static DataStore g_datastore;
static UIContext g_ui;

static time_t g_last_reconnect_attempt = 0;
static int g_reconnect_attempts = 0;
static time_t g_disconnect_time = 0;      /* Time when disconnected (0 = connected) */
static bool g_data_reset_done = false;    /* Flag to prevent repeated resets */

/* Signal handler for SIGWINCH (terminal resize) */
static void sigwinch_handler(int signum)
{
    (void)signum;
    g_resize_pending = 1;
}

/* Signal handler for SIGINT/SIGTERM */
static void sigterm_handler(int signum)
{
    (void)signum;
    g_ui.running = false;
}

/* Setup signal handlers */
static void setup_signals(void)
{
    struct sigaction sa;

    /* SIGWINCH for terminal resize */
    sa.sa_handler = sigwinch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    /* SIGINT/SIGTERM for graceful shutdown */
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* Cleanup resources */
static void cleanup(void)
{
    ui_cleanup(&g_ui);
    logger_close(&g_logger);
    network_close(&g_network);
    datastore_free(&g_datastore);
}

/* Save snapshot to file */
static void save_snapshot(void)
{
    char filename[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(filename, sizeof(filename), "emulator_snap_%Y%m%d%H%M%S.tsnap", tm_info);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;
    }

    /* Header */
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "=== Snapshot: %s ===\n\n", time_str);

    const InfoData *info = datastore_get_info(&g_datastore);
    const IOData *io = datastore_get_io(&g_datastore);
    const CPUData *cpu = datastore_get_cpu(&g_datastore);
    const MemoryData *mem = datastore_get_memory(&g_datastore);

    /* Tab 1: Info */
    fprintf(fp, "=== TAB 1: Info ===\n");
    fprintf(fp, "Emulator    : %s %s\n", info->emu_type, info->emu_version);
    fprintf(fp, "Machine ID  : %s\n", info->machine_id);
    fprintf(fp, "Machine Name: %s\n", info->machine_name);
    fprintf(fp, "Machine Type: %s\n", info->machine_type);
    fprintf(fp, "CPU Type    : %s\n", info->cpu_type);
    fprintf(fp, "Status Mode : %s\n", info->status_mode);
    fprintf(fp, "Video Mode  : %s\n", info->video_mode);
    fprintf(fp, "Total Msgs  : %lu\n", g_datastore.total_messages);
    fprintf(fp, "\n");

    /* Tab 2: IO */
    fprintf(fp, "=== TAB 2: IO ===\n");
    fprintf(fp, "Entries: %d\n", io->count);
    const IOEntry *sorted[IO_MAX_ENTRIES];
    int io_count = datastore_io_get_sorted(io, sorted, io->count);
    for (int i = 0; i < io_count; i++) {
        const IOEntry *e = sorted[i];
        if (e->addr[0]) {
            fprintf(fp, "[%s] %s @%s = %s\n", e->sec, e->fld, e->addr, e->val);
        } else if (e->idx >= 0) {
            fprintf(fp, "[%s] %s #%d = %s\n", e->sec, e->fld, e->idx, e->val);
        } else {
            fprintf(fp, "[%s] %s = %s\n", e->sec, e->fld, e->val);
        }
    }
    fprintf(fp, "\n");

    /* Tab 3: CPU */
    fprintf(fp, "=== TAB 3: CPU ===\n");
    for (int i = 0; i < cpu->count; i++) {
        fprintf(fp, "[%s] %s = %s\n",
                cpu->entries[i].sec, cpu->entries[i].fld, cpu->entries[i].val);
    }
    fprintf(fp, "\n");

    /* Tab 4: Memory (complete dump) */
    fprintf(fp, "=== TAB 4: Memory ===\n");
    fprintf(fp, "Lines: %d\n", mem->count);
    if (mem->count > 0) {
        fprintf(fp, "Address range: %04X - %04X\n\n", mem->min_addr, mem->max_addr);
    }

    for (int i = 0; i < mem->count; i++) {
        const MemLine *line = datastore_memory_get_line(mem, i);
        if (!line) continue;

        fprintf(fp, "%04X: ", line->address);
        for (int j = 0; j < MEM_BYTES_PER_LINE; j++) {
            if (line->valid[j]) {
                fprintf(fp, "%02X ", line->data[j]);
            } else {
                fprintf(fp, "-- ");
            }
        }

        /* ASCII representation */
        fprintf(fp, " |");
        for (int j = 0; j < MEM_BYTES_PER_LINE; j++) {
            if (line->valid[j]) {
                unsigned char c = line->data[j];
                fprintf(fp, "%c", (c >= 32 && c < 127) ? c : '.');
            } else {
                fprintf(fp, " ");
            }
        }
        fprintf(fp, "|\n");
    }

    fprintf(fp, "\n=== End of Snapshot ===\n");
    fclose(fp);

    logger_write_fmt(&g_logger, "Snapshot saved: %s", filename);
}

/* Attempt to connect/reconnect to debug server */
static int try_connect(void)
{
    if (network_is_connected(&g_network)) {
        return 0;
    }

    time_t now = time(NULL);

    /* Check if enough time has passed since last attempt */
    if (g_last_reconnect_attempt > 0 &&
        (now - g_last_reconnect_attempt) < RECONNECT_INTERVAL_SEC) {
        return -1;
    }

    g_last_reconnect_attempt = now;
    g_reconnect_attempts++;

    /* Check max attempts (0 = unlimited) */
    if (RECONNECT_MAX_ATTEMPTS > 0 &&
        g_reconnect_attempts > RECONNECT_MAX_ATTEMPTS) {
        return -1;
    }

    /* Show connecting message */
    ui_set_status_message(&g_ui, "Connecting to %s:%d...",
                          g_config.debug_address, g_config.debug_port);

    int ret = network_connect(&g_network, g_config.debug_address, g_config.debug_port);
    if (ret == NETWORK_OK) {
        logger_write_fmt(&g_logger, "Connected to %s:%d",
                        g_config.debug_address, g_config.debug_port);
        ui_set_connected(&g_ui, true, NULL);
        ui_set_status_message(&g_ui, "Connected to %s:%d",
                              g_config.debug_address, g_config.debug_port);
        g_reconnect_attempts = 0;
        g_disconnect_time = 0;        /* Reset disconnect timer */
        g_data_reset_done = false;    /* Reset the reset flag */
        return 0;
    }

    ui_set_status_message(&g_ui, "Failed to connect to %s:%d (retry in %ds)",
                          g_config.debug_address, g_config.debug_port,
                          RECONNECT_INTERVAL_SEC);
    return -1;
}

/* Process network data */
static void process_network_data(void)
{
    char line[NETWORK_MAX_LINE_SIZE];
    ParsedData data;

    while (network_read_line(&g_network, line, sizeof(line)) == NETWORK_OK) {
        /* Log raw line if logging enabled */
        logger_write(&g_logger, line);

        /* Parse the JSON line */
        if (parser_parse_line(line, &data) == 0) {
            /* Store in data store */
            datastore_process(&g_datastore, &data);

            /* Update UI connection status with emulator type from first message */
            if (g_ui.emu_type[0] == '\0' && data.emu[0] != '\0') {
                ui_set_connected(&g_ui, true, data.emu);
            }

            ui_mark_dirty(&g_ui);
        } else {
            g_datastore.parse_errors++;
        }
    }
}

int main(int argc, char **argv)
{
    int ret;

    /* Initialize config with defaults */
    config_init(&g_config);

    /* Parse command line arguments first (they have priority) */
    ret = config_parse_args(&g_config, argc, argv);
    if (ret == 1) {
        /* --help requested */
        config_print_usage(argv[0]);
        return 0;
    }
    if (ret < 0) {
        fprintf(stderr, "Error parsing arguments. Use --help for usage.\n");
        return 1;
    }

    /* Load config file (won't override CLI args) */
    ret = config_load(&g_config);
    if (ret < 0) {
        /* No config file found, create default */
        config_create_default();
    }

    /* Initialize data store */
    datastore_init(&g_datastore);

    /* Initialize logger */
    ret = logger_init(&g_logger, g_config.log_all);
    if (ret < 0 && g_config.log_all) {
        fprintf(stderr, "Warning: Failed to initialize logger\n");
    }

    /* Setup signal handlers */
    setup_signals();

    /* Initialize UI first (before network connection) */
    ret = ui_init(&g_ui, &g_datastore);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize UI\n");
        datastore_free(&g_datastore);
        logger_close(&g_logger);
        return 1;
    }

    /* Initialize network */
    network_init(&g_network);

    /* Initial connection attempt */
    ui_set_connected(&g_ui, false, NULL);
    try_connect();

    /* Main event loop */
    fd_set read_fds;
    struct timeval tv;

    while (ui_is_running(&g_ui)) {
        /* Handle pending resize */
        if (g_resize_pending) {
            g_resize_pending = 0;
            ui_resize(&g_ui);
        }

        /* Update caps lock status periodically */
        ui_update_caps_lock(&g_ui);

        /* Try to reconnect if disconnected */
        if (!network_is_connected(&g_network)) {
            try_connect();
        }

        int net_fd = network_get_fd(&g_network);

        /* Setup select for both stdin and network */
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        if (net_fd >= 0 && network_is_connected(&g_network)) {
            FD_SET(net_fd, &read_fds);
        }

        tv.tv_sec = 0;
        tv.tv_usec = 50000;  /* 50ms timeout */

        int max_fd = STDIN_FILENO;
        if (net_fd > max_fd) max_fd = net_fd;

        ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ret > 0) {
            /* Check for keyboard input */
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                int ch = getch();
                int result = ui_handle_input(&g_ui, ch);

                /* Handle special return values */
                if (result == 's' || result == 'S') {
                    save_snapshot();
                }
            }

            /* Check for network data */
            if (net_fd >= 0 && FD_ISSET(net_fd, &read_fds)) {
                process_network_data();

                /* Check if connection lost */
                if (!network_is_connected(&g_network)) {
                    ui_set_connected(&g_ui, false, NULL);
                    ui_set_status_message(&g_ui, "Connection lost, will retry...");
                    logger_write_fmt(&g_logger, "Connection lost, will retry...");
                    if (g_disconnect_time == 0) {
                        g_disconnect_time = time(NULL);  /* Record disconnect time */
                    }
                }
            }
        }

        /* Check if data reset is needed after prolonged disconnect */
        if (g_disconnect_time > 0 && !g_data_reset_done) {
            time_t now = time(NULL);
            if (now - g_disconnect_time >= DISCONNECT_RESET_SEC) {
                datastore_clear(&g_datastore);
                g_data_reset_done = true;
                ui_mark_dirty(&g_ui);
                ui_set_status_message(&g_ui, "Data reset after %d seconds disconnect",
                                      DISCONNECT_RESET_SEC);
                logger_write_fmt(&g_logger, "Data reset after %d seconds disconnect",
                                DISCONNECT_RESET_SEC);
            }
        }

        /* Update UI */
        ui_update(&g_ui);
    }

    logger_write_fmt(&g_logger, "Shutting down");
    cleanup();

    return 0;
}
