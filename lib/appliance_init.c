/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "test_runner.h"

int vv_appliance_main(int argc, char **argv, const struct vv_test_suite *suite)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--list")) {
        vv_list_tests(suite);
        return 0;
    }
    if (getpid() != 1) {
        fprintf(stderr, "error: execute only as appliance /init; --list is safe on the host\n");
        return 2;
    }
    const char *paths[] = {"/proc", "/sys", "/dev"};
    const char *types[] = {"proc", "sysfs", "devtmpfs"};
    int ready = 1;
    for (unsigned i = 0; i < 3; ++i) {
        if (mount(types[i], paths[i], types[i], 0, NULL) != 0) {
            printf("vv-infra mount %s: expected 0, actual -1 errno=%d (%s)\n",
                   paths[i], errno, strerror(errno));
            ready = 0;
        }
    }
    char cmdline[4096] = {0};
    FILE *stream = fopen("/proc/cmdline", "r");
    if (!stream || !fgets(cmdline, sizeof(cmdline), stream) ||
        !strchr(cmdline, '\n')) {
        printf("vv-infra cannot read complete /proc/cmdline: errno=%d\n", errno);
        ready = 0;
    }
    if (stream)
        fclose(stream);
    char *selection = NULL, *shutdown = NULL, *save;
    const char *token = NULL;
    for (char *arg = strtok_r(cmdline, " \n", &save); arg;
         arg = strtok_r(NULL, " \n", &save)) {
        if (!strncmp(arg, "vv.tests=", 9)) {
            if (selection)
                ready = 0;
            selection = arg + 9;
        } else if (!strncmp(arg, "vv.run=", 7)) {
            if (token)
                ready = 0;
            token = arg + 7;
        } else if (!strncmp(arg, "vv.shutdown=", 12)) {
            if (shutdown)
                ready = 0;
            shutdown = arg + 12;
        }
    }
    if (shutdown && strcmp(shutdown, "host") && strcmp(shutdown, "poweroff")) {
        printf("vv-infra invalid vv.shutdown mode\n");
        ready = 0;
    }
    if (!token || !*token) {
        printf("vv-infra missing vv.run token: launch through the host runner\n");
        token = "missing";
        ready = 0;
    }
    struct utsname kernel;
    if (uname(&kernel) == 0)
        printf("vv-kernel %s %s\n", kernel.release, kernel.version);
    vv_run_suite(suite, selection, token, ready);
    sync();
    if (shutdown && !strcmp(shutdown, "host")) {
        printf("vv-appliance-stop %s\n", token);
        for (;;)
            pause();
    }
    if (reboot(RB_POWER_OFF) != 0)
        printf("vv-infra poweroff failed errno=%d; supervisor must stop appliance\n", errno);
    for (;;)
        pause();
    return 2;
}
