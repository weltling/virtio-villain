/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0051: blk_request_max_chain_length
 *
 * Submit a block READ request that spans the maximum descriptor chain
 * length. Header + many data descriptors + status, using all available
 * descriptors in the ring for one request. Tests device processing
 * of very long chains near the queue size limit.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_max_chain(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    uint16_t qsz = vr->size;
    /* Use at most 128 descriptors (or queue size - whichever is smaller) */
    uint16_t chain_len = qsz < 128 ? qsz : 128;

    /* Desc 0: header */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);

    /* Descs 1..chain_len-2: data (each 512 bytes = 1 sector) */
    for (uint16_t i = 1; i < chain_len - 1; i++) {
        uint16_t next = i + 1;
        uint16_t flags = VRING_DESC_F_WRITE;
        if (i < chain_len - 2)
            flags |= VRING_DESC_F_NEXT;
        vring_raw_set_desc(vr, i, data_phys, 512, flags, next);
    }

    /* Last desc: status */
    vring_raw_set_desc(vr, chain_len - 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);
    /* Fix second-to-last to chain to status */
    if (chain_len > 2) {
        vring_raw_set_desc(vr, chain_len - 2, data_phys, 512,
                           VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
                           chain_len - 1);
    }

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0051, VIRTIO_PCI_DEVICE_BLK, test_blk_max_chain,
              "Block request with maximum descriptor chain length",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
