/*
 * network.h - TCP socket communication for rdemonitor
 *
 * Handles connection to the emulator debug server and line-based data reception.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

#define NETWORK_RECV_BUFFER_SIZE 8192
#define NETWORK_MAX_LINE_SIZE    4096

typedef enum {
    NETWORK_OK = 0,
    NETWORK_ERROR = -1,
    NETWORK_DISCONNECTED = -2,
    NETWORK_NO_DATA = -3
} NetworkResult;

typedef struct {
    int  sockfd;
    char recv_buffer[NETWORK_RECV_BUFFER_SIZE];
    int  buffer_len;
    bool connected;
    char address[256];
    int  port;
} NetworkContext;

/*
 * Initialize network context
 */
void network_init(NetworkContext *ctx);

/*
 * Connect to debug server
 * Returns: NETWORK_OK on success, NETWORK_ERROR on failure
 */
int network_connect(NetworkContext *ctx, const char *address, int port);

/*
 * Check if data is available (non-blocking)
 * timeout_ms: timeout in milliseconds (0 = non-blocking, -1 = blocking)
 * Returns: 1 if data available, 0 if timeout, -1 on error
 */
int network_data_available(NetworkContext *ctx, int timeout_ms);

/*
 * Read a single line from the socket (up to newline)
 * This function buffers data internally and returns complete lines.
 * Returns: NETWORK_OK if line read, NETWORK_NO_DATA if no complete line,
 *          NETWORK_DISCONNECTED if connection lost, NETWORK_ERROR on error
 */
int network_read_line(NetworkContext *ctx, char *line, int max_len);

/*
 * Close the connection
 */
void network_close(NetworkContext *ctx);

/*
 * Get connection status
 */
bool network_is_connected(const NetworkContext *ctx);

/*
 * Get socket file descriptor (for use with select/poll)
 */
int network_get_fd(const NetworkContext *ctx);

#endif /* NETWORK_H */
