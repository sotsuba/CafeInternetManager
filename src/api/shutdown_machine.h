#ifndef __shutdown_machine_h__
#define __shutdown_machine_h__

#include <stdlib.h>
#include <stdio.h>

void shutdown() {
    int ret = system("systemctl poweroff");

    if (ret == -1) {
        perror("system");
    }

    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        printf("Shutdown requested\n");
    } else {
        printf("Failed to request shutdown, exit code = %d\n", WEXITSTATUS(ret));
    }
}


#endif 