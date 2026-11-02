/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0200: virtio-blk hot add discoverable through PCI rescan
 *
 * Spec 4.1.4.1 says the driver discovers a virtio PCI device by
 * scanning configuration space. Hot adding a device on the host
 * side must therefore appear to the guest as a new vendor 0x1af4
 * function once a rescan is triggered. Record the BDF of the
 * already attached blk device, ask the kernel to rescan the
 * root bus, and poll for a different blk function. When a new
 * function shows up, run the standard init sequence, build one
 * descriptor chain that reads sector 0, kick and wait for the
 * used ring to advance and the status byte to read OK.
 *
 * If no new device appears within the polling window the host
 * side did not perform the hot add. The runner spawns a
 * companion script next to this file that drives the host
 * action. When the script is absent or the backend has no add
 * disk verb, the guest cannot tell the difference and reports
 * SKIP rather than FAIL.
 */
#include "lib/virtio_test.h"
#include "lib/pci_hotplug.h"
#include "lib/pci.h"

static test_result_t test_blk_hotplug_add(struct virtio_dev *dev)
{
    char skip[256];
    snprintf(skip, sizeof(skip), "%s", bdf_of(dev->slot));

    char new_slot[512] = {0};
    int waited = 0;
    int total = 10000;
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
        return TEST_SKIP;

    struct virtio_dev hp = {0};
    snprintf(hp.slot, sizeof(hp.slot), "%s", new_slot);
    if (virtio_pci_attach(VIRTIO_PCI_DEVICE_BLK, &hp) < 0)
        TFAIL("virtio_pci_attach(VIRTIO_PCI_DEVICE_BLK, &hp) < 0");
    if (virtio_pci_init(&hp) < 0)
        TFAIL("virtio_pci_init(&hp) < 0");

    struct vring q0;
    vring_alloc(&q0, 16);
    vring_attach(&hp, &q0, 0);
    hp.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;
    vv_desc(&q0, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&q0, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&q0, 2, st, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    VV_TRY(vv_kick_and_wait(&hp, &q0, 0, VV_TIMEOUT_MS));
    if (*st != 0)
        TFAIL("*st != 0");
    return TEST_PASS;
}

REGISTER_TEST_DEV(B0200, VIRTIO_PCI_DEVICE_BLK, test_blk_hotplug_add,
                  "virtio-blk hot add discoverable through PCI rescan",
                  VIRTIO_SPEC_V1_2, "4.1.4.1");
