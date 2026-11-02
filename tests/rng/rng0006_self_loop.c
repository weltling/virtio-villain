/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0006: RNG chain with self-loop.
 *
 * Build a writable requestq chain whose head sets
 * VRING_DESC_F_NEXT|WRITE and points back at itself. The device
 * must detect the cycle rather than walk the chain forever.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_self_loop(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 32, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0006, VIRTIO_PCI_DEVICE_RNG, test_rng_self_loop,
              "RNG chain self-loop",
              VIRTIO_SPEC_V1_2, "5.4.6");
