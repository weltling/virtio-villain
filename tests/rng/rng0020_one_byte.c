/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0020: rng_one_byte_buffer
 *
 * Request exactly one byte of entropy. Spec 5.4.6 says the device
 * writes random data into the buffer. A single byte buffer is a
 * boundary condition that must not confuse the device model.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_one_byte(struct virtio_dev *dev,
                                       struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0;

    vv_desc(vr, 0, buf, 1, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0020, VIRTIO_PCI_DEVICE_RNG, test_rng_one_byte,
              "Single byte entropy request",
              VIRTIO_SPEC_V1_2, "5.4.6");
