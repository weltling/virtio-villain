/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0008: pci_write_queue_enable_zero
 *
 * Write 0 to queue_enable after the queue has been enabled and is live.
 * Spec 4.1.4.3.2: driver MUST NOT write a 0 to queue_enable. This
 * attempts to "disable" a live queue, which may cause the VMM to
 * tear down queue state while I/O is in flight.
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

static test_result_t test_pci_write_queue_enable_zero(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    /* Queue is live (harness enabled it). Write 0 to queue_enable. */
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_enable = 0;
    __sync_synchronize();
    usleep(10000);

    /* Now try to use the "disabled" queue */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0008, VIRTIO_PCI_DEVICE_BLK, test_pci_write_queue_enable_zero,
              "Write queue_enable=0 after queue is live",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
