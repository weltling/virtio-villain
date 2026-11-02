/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0015: watchdog_indirect_oob_next
 *
 * Use an indirect table whose first entry is a valid ping and
 * whose second entry references next index 99, far past the
 * three slot table. Spec 2.7.5.3 makes the table self contained;
 * the device must reject the chain rather than processing the
 * first entry in isolation.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_indirect_oob_next(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *ping = vv_alloc_pages(1);
    struct vring_desc *ind = vv_alloc_pages(1);

    ind[0].addr = vv_virt_to_phys(ping);
    ind[0].len = 1;
    ind[0].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    ind[0].next = 1;

    ind[1].addr = vv_virt_to_phys(ping);
    ind[1].len = 1;
    ind[1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    ind[1].next = 99;

    ind[2].addr = vv_virt_to_phys(ping);
    ind[2].len = 1;
    ind[2].flags = VRING_DESC_F_WRITE;
    ind[2].next = 0;

    vv_desc(vr, 0, ind, 3 * sizeof(struct vring_desc), VRING_DESC_F_INDIRECT,
            0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0015, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_indirect_oob_next,
              "Indirect ping table with out of bounds NEXT",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
