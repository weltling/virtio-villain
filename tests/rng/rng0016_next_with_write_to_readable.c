/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0016: rng_next_with_write_to_readable
 *
 * Submit a chain whose first descriptor is writable plus NEXT and
 * the second descriptor is readable. Spec 2.7.5.2 says all device
 * writable descriptors must follow all device readable ones in a
 * chain. The device must reject or stay silent.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_next_write_then_read(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint8_t *out = vv_alloc_pages(1);
    uint8_t *in = vv_alloc_pages(1);

    vv_desc(vr, 0, out, 64, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, in, 16, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RNG0016, VIRTIO_PCI_DEVICE_RNG, test_rng_next_write_then_read,
              "RNG chain with writable then readable order",
              VIRTIO_SPEC_V1_2, "2.7.5.2");
