#ifndef __DISCOVERY_H__
#define __DISCOVERY_H__

#include "platform.h"
#include <stdint.h>
#include <time.h>

#define DISCOVERY_PORT 9999
#define DISCOVERY_MAGIC 0xCAFE1234
#define MAX_DISCOVERED_BACKENDS 16
#define DISCOVERY_INTERVAL 5
#define BACKEND_TIMEOUT 15

// Discovery packet format
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct {
  uint32_t magic;
  uint16_t service_port;
  char advertised_hostname[64];
}
#ifndef _MSC_VER
__attribute__((packed))
#endif
discovery_packet_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

// Discovered backend info
typedef struct {
  char host[256];
  int port;
  time_t last_seen;
  char service_name[64];
  int active;
} discovered_backend_t;

// Initialize discovery system
int discovery_init(void);

// Cleanup discovery system
void discovery_cleanup(void);

// Discovery thread
#ifdef _WIN32
unsigned __stdcall discovery_thread_fn(void *arg);
#else
void *discovery_thread_fn(void *arg);
#endif

// Get list of discovered backends
int get_discovered_backends(discovered_backend_t *backends, int max_count);

#endif // __DISCOVERY_H__
