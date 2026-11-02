/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0014: RNG max-length chain.
 *
 * Build a writable chain that uses every slot in the ring linked
 * via VRING_DESC_F_NEXT. The device must walk the full chain
 * without overflowing internal limits.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_max_chain(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    uint16_t n = vr->size;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t flags = VRING_DESC_F_WRITE |
                         ((i + 1 < n) ? VRING_DESC_F_NEXT : 0);
        uint16_t next  = (i + 1 < n) ? (uint16_t)(i + 1) : 0;
        vring_raw_set_desc(vr, i, buf_phys + i * 4, 4, flags, next);
    }
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0014, VIRTIO_PCI_DEVICE_RNG, test_rng_max_chain,
              "RNG max-length chain",
              VIRTIO_SPEC_V1_2, "5.4.6");
