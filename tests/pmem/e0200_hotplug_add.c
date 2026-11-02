/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0200: virtio-pmem hot add discoverable through PCI rescan.
 *
 * Mirrors B0200 for the pmem device. Records the BDF of the
 * already attached pmem function, asks the kernel to rescan, and
 * polls for a new pmem function. On success runs virtio_pci_attach
 * on the new function and asserts a fresh device_status of zero
 * per spec 4.1.4.3.1.
 */
#include "lib/virtio_test.h"
#include "lib/pci_hotplug.h"
#include "lib/pci.h"

static test_result_t test_pmem_hotplug_add(struct virtio_dev *dev)
{
    char skip[256];
    snprintf(skip, sizeof(skip), "%s", bdf_of(dev->slot));

    char new_slot[512] = {0};
    int waited = 0;
    int total = 15000;
    int step = 100;
    while (waited < total) {
        if (trigger_pci_rescan() == 0
            && find_other_dev(VIRTIO_PCI_DEVICE_PMEM, skip,
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
    if (virtio_pci_attach(VIRTIO_PCI_DEVICE_PMEM, &hp) < 0)
        TFAIL("virtio_pci_attach(VIRTIO_PCI_DEVICE_PMEM, &hp) < 0");
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

REGISTER_TEST_DEV(E0200, VIRTIO_PCI_DEVICE_PMEM, test_pmem_hotplug_add,
                  "virtio-pmem hot add presents a fresh device_status",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.1");
