/*
 * network.c - TCP socket communication implementation
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

void network_init(NetworkContext *ctx)
{
    ctx->sockfd = -1;
    ctx->buffer_len = 0;
    ctx->connected = false;
    memset(ctx->recv_buffer, 0, sizeof(ctx->recv_buffer));
    memset(ctx->address, 0, sizeof(ctx->address));
    ctx->port = 0;
}

int network_connect(NetworkContext *ctx, const char *address, int port)
{
    struct addrinfo hints, *result, *rp;
    char port_str[16];
    int ret;

    /* Close existing connection if any */
    if (ctx->sockfd >= 0) {
        network_close(ctx);
    }

    /* Store connection info */
    strncpy(ctx->address, address, sizeof(ctx->address) - 1);
    ctx->address[sizeof(ctx->address) - 1] = '\0';
    ctx->port = port;

    /* Prepare hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */

    snprintf(port_str, sizeof(port_str), "%d", port);

    ret = getaddrinfo(address, port_str, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return NETWORK_ERROR;
    }

    /* Try each address until we successfully connect */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        ctx->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (ctx->sockfd < 0) {
            continue;
        }

        if (connect(ctx->sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* Success */
        }

        close(ctx->sockfd);
        ctx->sockfd = -1;
    }

    freeaddrinfo(result);

    if (ctx->sockfd < 0) {
        return NETWORK_ERROR;
    }

    /* Set socket to non-blocking mode */
    int flags = fcntl(ctx->sockfd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(ctx->sockfd, F_SETFL, flags | O_NONBLOCK);
    }

    ctx->connected = true;
    ctx->buffer_len = 0;

    return NETWORK_OK;
}

int network_data_available(NetworkContext *ctx, int timeout_ms)
{
    if (!ctx->connected || ctx->sockfd < 0) {
        return -1;
    }

    /* Check if we already have a complete line in buffer */
    if (memchr(ctx->recv_buffer, '\n', ctx->buffer_len) != NULL) {
        return 1;
    }

    fd_set readfds;
    struct timeval tv;
    struct timeval *tvp = NULL;

    FD_ZERO(&readfds);
    FD_SET(ctx->sockfd, &readfds);

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }

    int ret = select(ctx->sockfd + 1, &readfds, NULL, NULL, tvp);
    if (ret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return -1;
    }

    return (ret > 0 && FD_ISSET(ctx->sockfd, &readfds)) ? 1 : 0;
}

int network_read_line(NetworkContext *ctx, char *line, int max_len)
{
    if (!ctx->connected || ctx->sockfd < 0) {
        return NETWORK_DISCONNECTED;
    }

    /* Try to read more data into buffer */
    if (ctx->buffer_len < NETWORK_RECV_BUFFER_SIZE - 1) {
        ssize_t n = recv(ctx->sockfd,
                        ctx->recv_buffer + ctx->buffer_len,
                        NETWORK_RECV_BUFFER_SIZE - 1 - ctx->buffer_len,
                        0);

        if (n > 0) {
            ctx->buffer_len += n;
            ctx->recv_buffer[ctx->buffer_len] = '\0';
        }
        else if (n == 0) {
            /* Connection closed by peer */
            ctx->connected = false;
            return NETWORK_DISCONNECTED;
        }
        else {
            /* n < 0 */
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ctx->connected = false;
                return NETWORK_ERROR;
            }
            /* EAGAIN/EWOULDBLOCK means no data available right now */
        }
    }

    /* Look for a complete line in buffer */
    char *newline = memchr(ctx->recv_buffer, '\n', ctx->buffer_len);
    if (newline == NULL) {
        return NETWORK_NO_DATA;
    }

    /* Calculate line length (excluding newline) */
    int line_len = newline - ctx->recv_buffer;
    if (line_len >= max_len) {
        line_len = max_len - 1;
    }

    /* Copy line to output */
    memcpy(line, ctx->recv_buffer, line_len);
    line[line_len] = '\0';

    /* Remove \r if present (for CRLF line endings) */
    if (line_len > 0 && line[line_len - 1] == '\r') {
        line[line_len - 1] = '\0';
    }

    /* Shift remaining data in buffer */
    int consumed = (newline - ctx->recv_buffer) + 1; /* +1 for newline */
    int remaining = ctx->buffer_len - consumed;
    if (remaining > 0) {
        memmove(ctx->recv_buffer, newline + 1, remaining);
    }
    ctx->buffer_len = remaining;
    ctx->recv_buffer[ctx->buffer_len] = '\0';

    return NETWORK_OK;
}

void network_close(NetworkContext *ctx)
{
    if (ctx->sockfd >= 0) {
        close(ctx->sockfd);
        ctx->sockfd = -1;
    }
    ctx->connected = false;
    ctx->buffer_len = 0;
}

bool network_is_connected(const NetworkContext *ctx)
{
    return ctx->connected;
}

int network_get_fd(const NetworkContext *ctx)
{
    return ctx->sockfd;
}
