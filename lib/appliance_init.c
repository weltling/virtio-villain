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

static int read_smbios_value(const char *path, char *value, size_t size)
{
    FILE *stream = fopen(path, "r");
    if (!stream) {
        printf("vv-infra cannot read %s: errno=%d (%s)\n",
               path, errno, strerror(errno));
        return 0;
    }
    int read = fgets(value, size, stream) != NULL;
    size_t length = strlen(value);
    int valid = read && length > 1 && value[length - 1] == '\n';
    if (fclose(stream) != 0)
        valid = 0;
    if (!valid) {
        printf("vv-infra missing or incomplete SMBIOS value: %s\n", path);
        return 0;
    }
    value[length - 1] = '\0';
    return 1;
}

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
    char *selection = NULL, *token = NULL, *shutdown = NULL, *config = NULL, *save;
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
        } else if (!strncmp(arg, "vv.config=", 10)) {
            if (config) {
                printf("vv-infra duplicate vv.config option\n");
                ready = 0;
            }
            config = arg + 10;
        }
    }
    char smbios_token[64] = {0}, smbios_suffix[34] = {0}, smbios_selection[34] = {0};
    if (config) {
        if (strcmp(config, "smbios") || token || selection) {
            printf("vv-infra invalid or mixed vv.config selection\n");
            ready = 0;
        } else {
            const char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
            if (read_smbios_value("/sys/class/dmi/id/product_serial",
                                  smbios_token, sizeof(smbios_token)) &&
                strlen(smbios_token) <= 32 &&
                read_smbios_value("/sys/class/dmi/id/product_version",
                                  smbios_suffix, sizeof(smbios_suffix)) &&
                !strncmp(smbios_suffix, "vv:", 3) &&
                strlen(smbios_token) + strlen(smbios_suffix + 3) <= 61) {
                strcat(smbios_token, smbios_suffix + 3);
            } else {
                smbios_token[0] = '\0';
            }
            if (*smbios_token &&
                strspn(smbios_token,
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") ==
                strlen(smbios_token)) {
                token = smbios_token;
            } else {
                printf("vv-infra invalid SMBIOS run token\n");
                ready = 0;
            }
            if (read_smbios_value("/sys/class/dmi/id/product_sku",
                                  smbios_selection, sizeof(smbios_selection))) {
                int valid = 1;
                for (char *entry = smbios_selection; *entry; ++entry) {
                    if (*entry == '+')
                        *entry = ',';
                    else if (!strchr(allowed, *entry))
                        valid = 0;
                }
                if (!valid) {
                    printf("vv-infra invalid SMBIOS test selection\n");
                    ready = 0;
                }
                selection = smbios_selection;
            } else {
                ready = 0;
            }
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
