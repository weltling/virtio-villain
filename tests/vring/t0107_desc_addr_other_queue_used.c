/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0107: desc_addr_other_queue_used
 *
 * Point a writable data descriptor on queue 0 at the used ring of a
 * second queue. The used ring is device owned, so a blk read that
 * DMAs disk bytes into queue 1's used ring lets the guest forge used
 * entries on a queue it is not driving. The device must not honor an
 * address that lands on another queue's used ring.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_other_queue_used(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP; /* need a second queue to target */

    /* Bring up queue 1 so its used ring is live memory. */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Data descriptor writes 512 bytes into queue 1's used ring. */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vr2.used_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(T0107, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_other_queue_used,
              "Descriptor address pointing into another queue's used ring",
              VIRTIO_SPEC_V1_2, "2.7.5",
              0, 2);
