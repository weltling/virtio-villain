/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0012: Balloon inflate with writable flag.
 *
 * The inflate queue takes only device-readable buffers per spec
 * 5.5.6.1. Submit one with VRING_DESC_F_WRITE set; device must
 * reject the violation.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_writable_flag(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0x1000;

    vv_desc(vr, 0, pfns, 4, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(L0012, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_writable_flag,
              "Inflate with WRITE flag set",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
