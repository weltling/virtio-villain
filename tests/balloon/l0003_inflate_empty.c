/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0003: Balloon inflate with zero PFNs (empty descriptor).
 *
 * Submit an inflate request with zero-length data. The device
 * must handle this gracefully.
 *
 * Spec 5.5.6.1: Robustness test.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_inflate_empty(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Zero-length descriptor */
    vv_desc(vr, 0, buf, 0, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(L0003, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_empty,
              "Inflate with zero-length PFN array",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
