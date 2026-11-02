/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0013: Balloon inflate single-byte length.
 *
 * Submit a descriptor of length 1 byte (smaller than one PFN).
 * The device must reject or treat as zero PFNs without crashing.
 *
 * Spec 5.5.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_short_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0x1000;

    vv_desc(vr, 0, pfns, 1, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(L0013, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_short_len,
              "Inflate single-byte length",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
