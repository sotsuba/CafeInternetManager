#ifndef __THREAD_H__
#define __THREAD_H__

#include "platform.h"

// Thread function declarations
#ifdef _WIN32
unsigned __stdcall ws_thread_fn(void *arg);
unsigned __stdcall backend_thread_fn(void *arg);
unsigned __stdcall monitor_thread_fn(void *arg);
#else
void *ws_thread_fn(void *arg);
void *backend_thread_fn(void *arg);
void *monitor_thread_fn(void *arg);
#endif

#endif // __THREAD_H__
