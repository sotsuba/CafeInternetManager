#ifndef __UTILS_H__
#define __UTILS_H__

#include "platform.h"
#include <stdint.h>

// Returns current time in nanoseconds since an arbitrary epoch (monotonic)
// (Note: Already defined as inline in platform.h, but kept here for linkage if needed)
#ifndef _WIN32
uint64_t get_time_ns(void);
#endif

// Set a socket to non-blocking mode
// Returns 0 on success, -1 on failure
int set_nonblocking(socket_t sock);

// Disable Nagle's algorithm on a TCP socket
int set_tcp_nodelay(socket_t sock);

// Signal/Wait helpers are platform-specific (Linux eventfd)
#ifndef _WIN32
void signal_eventfd(int efd);
void drain_eventfd(int efd);
#endif

#endif // __UTILS_H__
