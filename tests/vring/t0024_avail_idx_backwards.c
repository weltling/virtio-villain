/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0024: avail_idx_backwards
 *
 * Decrease avail.idx after submitting a request. The spec explicitly
 * states "the driver MUST NOT decrement the available idx." A VMM that
 * computes new entries as a wrapping subtraction (new_idx - old_idx)
 * using unsigned arithmetic will interpret a backwards move as a huge
 * forward jump and attempt to process 65535+ phantom entries.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_idx_backwards(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Set up a valid request at slot 0 */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);

    /*
     * First, advance avail.idx to 5 (pretending we submitted 5 entries)
     * and kick so the VMM records last_seen = 5.
     */
    vring_raw_set_avail_idx(vr, 5);
    virtio_pci_kick(dev, 0);
    usleep(200000);
    __sync_synchronize();

    /*
     * Now set avail.idx backwards to 2 and kick again.
     * The wrapping difference (2 - 5) as u16 = 65533 entries.
     */
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0024, VIRTIO_PCI_DEVICE_BLK, test_avail_idx_backwards,
              "Available idx moves backwards",
              VIRTIO_SPEC_V1_2, "2.7.6");
