/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0011: RNG indirect with unaligned length.
 *
 * Submit an indirect requestq descriptor whose len is 17, not a
 * multiple of sizeof(struct vring_desc)=16. Spec 2.7.7 requires
 * len to be a multiple of 16. The device must reject.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_indirect_unaligned(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);

    vv_desc(vr, 0, itab, 17, VRING_DESC_F_INDIRECT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0011, VIRTIO_PCI_DEVICE_RNG, test_rng_indirect_unaligned,
              "RNG indirect with unaligned length",
              VIRTIO_SPEC_V1_2, "2.7.7");
