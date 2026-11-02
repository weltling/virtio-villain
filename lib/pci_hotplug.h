/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Shared helpers for PCI hot plug tests. The host adds or removes a
 * function and the guest finds it through a sysfs rescan.
 */
#ifndef VV_HOTPLUG_H
#define VV_HOTPLUG_H

#include "tests/test.h"

#include <dirent.h>

/* Ask the kernel to rescan all PCI buses. Returns 0 on success. */
static inline int trigger_pci_rescan(void)
{
    int fd = open("/sys/bus/pci/rescan", O_WRONLY);
    if (fd < 0)
        return -1;
    int rc = (int)write(fd, "1", 1);
    close(fd);
    return rc < 0 ? -1 : 0;
}

/*
 * Find a virtio PCI function with the given device ID whose BDF
 * directory name differs from skip_bdf (NULL skips nothing). Returns 0
 * and fills slot with its sysfs path on success, -1 if none.
 */
static inline int find_other_dev(uint16_t devid, const char *skip_bdf,
                                 char *slot, size_t len)
{
    DIR *d = opendir("/sys/bus/pci/devices");
    if (!d)
        return -1;
    int found = -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        if (skip_bdf && strcmp(ent->d_name, skip_bdf) == 0)
            continue;
        char path[512];
        unsigned v, dev;
        FILE *f;
        snprintf(path, sizeof(path),
                 "/sys/bus/pci/devices/%s/vendor", ent->d_name);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%x", &v) != 1) { fclose(f); continue; }
        fclose(f);
        snprintf(path, sizeof(path),
                 "/sys/bus/pci/devices/%s/device", ent->d_name);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%x", &dev) != 1) { fclose(f); continue; }
        fclose(f);
        if (v == VIRTIO_PCI_VENDOR && dev == devid) {
            snprintf(slot, len, "/sys/bus/pci/devices/%s", ent->d_name);
            found = 0;
            break;
        }
    }
    closedir(d);
    return found;
}

/* Return the BDF part of a sysfs slot path. */
static inline const char *bdf_of(const char *slot)
{
    const char *s = strrchr(slot, '/');
    return s ? s + 1 : slot;
}

#endif /* VV_HOTPLUG_H */
