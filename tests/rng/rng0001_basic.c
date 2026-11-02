/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0001: RNG basic request.
 *
 * Submit a single writable descriptor to the entropy device's
 * requestq (queue 0). Per spec 5.4.6 the device must place at
 * least one random byte into the buffer and advance the used
 * ring.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_basic(struct virtio_dev *dev,
                                    struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 64, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0001, VIRTIO_PCI_DEVICE_RNG, test_rng_basic,
              "RNG basic request",
              VIRTIO_SPEC_V1_2, "5.4.6");
