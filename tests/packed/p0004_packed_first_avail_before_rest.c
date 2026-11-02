/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0004: packed_first_avail_before_rest
 *
 * Make the first descriptor in a chain available (set AVAIL bit)
 * before the subsequent descriptors are fully written. The device
 * may read garbage from not-yet-written descriptors.
 * Spec 2.8.17: driver MUST make the first descriptor available only
 * after all subsequent descriptors have been written.
 */
#include "lib/virtio_test.h"

static test_result_t test_packed_first_avail_before_rest(struct virtio_dev *dev,
                                                         struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Make desc[0] available FIRST (with AVAIL bit set),
     * but leave desc[1] and desc[2] as all-zeros initially.
     */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    __sync_synchronize();

    /* Kick before writing the rest of the chain */
    virtio_pci_kick(dev, 0);

    /* Now write the remaining descriptors (too late) */
    usleep(1000); /* small delay to widen the race window */
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    __sync_synchronize();

    usleep(500000);
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0004, VIRTIO_PCI_DEVICE_BLK, test_packed_first_avail_before_rest,
                     "First desc available before subsequent descs ready",
                     VIRTIO_SPEC_V1_2, "2.8.17");
