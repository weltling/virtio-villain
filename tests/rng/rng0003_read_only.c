/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0003: RNG read-only descriptor.
 *
 * Submit a requestq descriptor without VRING_DESC_F_WRITE. Per
 * spec 5.4.6.1 the driver MUST NOT place device-readable buffers
 * into the queue. The device must reject the descriptor rather
 * than write into a read-only mapping.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_read_only(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 64, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0003, VIRTIO_PCI_DEVICE_RNG, test_rng_read_only,
              "RNG read-only descriptor",
              VIRTIO_SPEC_V1_2, "5.4.6.1");
