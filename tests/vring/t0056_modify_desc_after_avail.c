/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0056: modify_desc_after_avail
 *
 * Place a valid descriptor in the available ring, kick, then
 * immediately rewrite the descriptor to point to a different address
 * and change its length. Tests whether the device reads the descriptor
 * atomically or may see a partial update.
 */
#include "lib/virtio_test.h"

static test_result_t test_modify_desc_after_avail(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    uint8_t *bogus = vv_alloc_pages(1);

    *status = 0xFF;

    /* Set up valid chain */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);
    __sync_synchronize();

    virtio_pci_kick(dev, 0);

    /* Race: immediately rewrite desc[1] to point elsewhere */
    vv_desc(vr, 1, bogus, 4096, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    __sync_synchronize();

    usleep(500000);

    /* Survival = pass */
    return TEST_PASS;
}

REGISTER_TEST(T0056, VIRTIO_PCI_DEVICE_BLK, test_modify_desc_after_avail,
              "Modify descriptor after making available",
              VIRTIO_SPEC_V1_2, "2.7.5");
