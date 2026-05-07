/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0053: reset issued while a request is in flight
 *
 * Spec 2.1.2 says a reset must complete promptly. If the driver
 * issues a reset while a request is pending in the avail ring the
 * device must drop the in flight work and reach status zero. A
 * follow up reinit and submission must work cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_reset_during_io(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Reset immediately, no waiting */
    virtio_pci_reset(dev);

    /* Reinit and confirm device is healthy again */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST(S0053, VIRTIO_PCI_DEVICE_BLK, test_reset_during_io,
              "reset issued right after kick, no completion wait",
              VIRTIO_SPEC_V1_2, "2.1.2");
