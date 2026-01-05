/**
 * Platform Abstraction Layer for Cross-Platform Gateway
 * Provides unified API for Linux and Windows
 */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
    // ===== Windows =====
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h>

    #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #endif

    // Socket types
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define IS_SOCKET_ERROR(x) ((x) == SOCKET_ERROR)
    #define SOCKET_ERRNO WSAGetLastError()

    // Close socket
    #define CLOSE_SOCKET(s) closesocket(s)

    // Non-blocking mode
    static inline int set_nonblocking(socket_t sock) {
        u_long mode = 1;
        return ioctlsocket(sock, FIONBIO, &mode);
    }

    // Thread types
    typedef HANDLE thread_t;
    typedef CRITICAL_SECTION mutex_t;
    typedef unsigned (__stdcall *thread_func_t)(void*);

    #define THREAD_CREATE(t, func, arg) \
        ((*(t) = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void*))(func), (void*)(arg), 0, NULL)) != NULL ? 0 : -1)

    #define THREAD_JOIN(t) (WaitForSingleObject((t), INFINITE), CloseHandle(t))
    #define MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
    #define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))

    // Atomic operations (using a prefix to avoid collision with stdatomic.h)
    typedef volatile long gw_atomic_bool;
    typedef volatile long gw_atomic_uint;
    #define GW_ATOMIC_VAR_INIT(x) (x)

    #define gw_atomic_store(p, v) InterlockedExchange((volatile long*)(p), (long)(v))
    #define gw_atomic_load(p) InterlockedCompareExchange((volatile long*)(p), 0, 0)
    #define gw_atomic_fetch_add(p, v) InterlockedExchangeAdd((volatile long*)(p), (long)(v))

    // Time
    static inline uint64_t get_time_ns(void) {
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        return (uint64_t)((double)counter.QuadPart * 1000000000.0 / (double)freq.QuadPart);
    }

    // Sleep
    #define sleep(s) Sleep((s) * 1000)
    #define usleep(us) Sleep((us) / 1000)

    // Signal handling placeholder
    #define SIGPIPE 0
    #define SIG_IGN NULL

#else
    // ===== Linux =====
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <pthread.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <signal.h>
    #include <stdatomic.h>

    // Socket types
    typedef int socket_t;
    #define INVALID_SOCKET_VAL (-1)
    #define IS_SOCKET_ERROR(x) ((x) < 0)
    #define SOCKET_ERRNO errno

    // Close socket
    #define CLOSE_SOCKET(s) close(s)

    // Non-blocking mode
    static inline int set_nonblocking(socket_t sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    // Thread types
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;

    #define THREAD_CREATE(t, func, arg) pthread_create((t), NULL, (void* (*)(void*))(func), (void*)(arg))
    #define THREAD_JOIN(t) pthread_join((t), NULL)
    #define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
    #define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
    #define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
    #define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))

    // Time
    static inline uint64_t get_time_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }

    // Atomic operations
    #define gw_atomic_bool atomic_bool
    #define gw_atomic_store(p, v) atomic_store((p), (v))
    #define gw_atomic_load(p) atomic_load((p))
    #define gw_atomic_fetch_add(p, v) atomic_fetch_add((p), (v))

#endif

// ===== Common =====

// Initialize platform (call once at startup)
static inline int platform_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[Platform] WSAStartup failed: %d\n", WSAGetLastError());
        return -1;
    }
    printf("[Platform] Winsock initialized\n");
#endif
    return 0;
}

// Cleanup platform (call at shutdown)
static inline void platform_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

#endif // __PLATFORM_H__
