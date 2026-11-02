/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0201: virtio-blk hot remove and re add does not leak device_status.
 *
 * The host removes a previously hot added blk function and then
 * adds it back under the same id. Spec 4.1.4.3.1 mandates that a
 * write of zero to device_status resets the device and that
 * subsequent reads of device_status return zero. This test waits
 * for a freshly hot added function to appear, attaches the
 * transport, issues the reset, polls device_status until it reads
 * zero or the timeout elapses, then asserts FEATURES_OK is clear
 * and queue_enable on queue 0 reads zero. Guest markers coordinate
 * discovery and removal so the final check runs only after readd.
 */
#include "lib/virtio_test.h"
#include "lib/pci_hotplug.h"
#include "lib/pci.h"

static test_result_t test_blk_hotplug_remove_readd(struct virtio_dev *dev,
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
            && find_other_dev(VIRTIO_PCI_DEVICE_BLK, skip,
                              new_slot, sizeof(new_slot)) == 0)
            break;
        usleep(step * 1000);
        waited += step;
        new_slot[0] = 0;
    }
    if (new_slot[0] == 0)
        TFAIL("first hot added disk did not appear");

    printf("vv-blk-added\n");
    fflush(stdout);
    waited = 0;
    while (waited < total && access(new_slot, F_OK) == 0) {
        usleep(step * 1000);
        waited += step;
    }
    if (access(new_slot, F_OK) == 0)
        TFAIL("hot added disk was not removed");

    printf("vv-blk-removed\n");
    fflush(stdout);
    new_slot[0] = 0;
    waited = 0;
    while (waited < total) {
        if (trigger_pci_rescan() == 0
            && find_other_dev(VIRTIO_PCI_DEVICE_BLK, skip,
                              new_slot, sizeof(new_slot)) == 0)
            break;
        usleep(step * 1000);
        waited += step;
        new_slot[0] = 0;
    }
    if (new_slot[0] == 0)
        TFAIL("hot disk did not reappear");

    struct virtio_dev hp = {0};
    snprintf(hp.slot, sizeof(hp.slot), "%s", new_slot);
    if (virtio_pci_attach(VIRTIO_PCI_DEVICE_BLK, &hp) < 0)
        TFAIL("virtio_pci_attach(VIRTIO_PCI_DEVICE_BLK, &hp) < 0");

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
    if (cfg->device_status & VIRTIO_STATUS_FEATURES_OK)
        TFAIL("cfg->device_status & VIRTIO_STATUS_FEATURES_OK");
    cfg->queue_select = 0;
    __sync_synchronize();
    if (cfg->queue_enable != 0)
        TFAIL("cfg->queue_enable != 0");
    return TEST_PASS;
}

REGISTER_TEST(B0201, VIRTIO_PCI_DEVICE_BLK, test_blk_hotplug_remove_readd,
              "virtio-blk hot remove and re add presents fresh device_status",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
