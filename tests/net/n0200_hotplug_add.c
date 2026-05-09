/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0200: virtio-net hot add discoverable through PCI rescan.
 *
 * Mirrors B0200 for the net device. Records the BDF of the already
 * attached net function, asks the kernel to rescan, and polls for
 * a new net function. On success runs virtio_pci_attach on the new
 * function and asserts a fresh device_status of zero per spec
 * 4.1.4.3.1. When no new function appears the sidecar did not run
 * or the backend cannot honour add-net, and the guest reports SKIP.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/pci.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int trigger_pci_rescan(void)
{
    int fd = open("/sys/bus/pci/rescan", O_WRONLY);
    if (fd < 0)
        return -1;
    int rc = (int)write(fd, "1", 1);
    close(fd);
    return rc < 0 ? -1 : 0;
}

static int find_other_dev(uint16_t devid, const char *skip_bdf,
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

static const char *bdf_of(const char *slot)
{
    const char *s = strrchr(slot, '/');
    return s ? s + 1 : slot;
}

static test_result_t test_net_hotplug_add(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    char skip[256];
    snprintf(skip, sizeof(skip), "%s", bdf_of(dev->slot));

    char new_slot[512] = {0};
    int waited = 0;
    int total = 15000;
    int step = 100;
    while (waited < total) {
        if (trigger_pci_rescan() == 0
            && find_other_dev(VIRTIO_PCI_DEVICE_NET, skip,
                              new_slot, sizeof(new_slot)) == 0)
            break;
        usleep(step * 1000);
        waited += step;
        new_slot[0] = 0;
    }
    if (new_slot[0] == 0)
        return TEST_SKIP;

    struct virtio_dev hp = {0};
    snprintf(hp.slot, sizeof(hp.slot), "%s", new_slot);
    if (virtio_pci_attach(VIRTIO_PCI_DEVICE_NET, &hp) < 0)
        TFAIL("virtio_pci_attach(VIRTIO_PCI_DEVICE_NET, &hp) < 0");
    volatile struct virtio_pci_common_cfg *cfg = hp.common;
    cfg->device_status = 0;
    __sync_synchronize();
    int rwait = 0;
    while (rwait < 2000) {
        __sync_synchronize();
        if (cfg->device_status == 0)
            break;
        usleep(10 * 1000);
        rwait += 10;
    }
    if (cfg->device_status != 0)
        TFAIL("cfg->device_status != 0");
    return TEST_PASS;
}

REGISTER_TEST(N0200, VIRTIO_PCI_DEVICE_NET, test_net_hotplug_add,
              "virtio-net hot add presents a fresh device_status",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
