/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0016: Pmem flush merged single descriptor.
 *
 * Place type and ret in one buffer with VRING_DESC_F_WRITE so the
 * device sees a single read+write descriptor instead of a chain.
 * Per spec 2.7.5 device-readable and device-writable bytes must
 * be in separate descriptors; the device should reject.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_merged(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint32_t *buf = vv_alloc_pages(1);
    buf[0] = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    vv_desc(vr, 0, buf, 8, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0016, VIRTIO_PCI_DEVICE_PMEM, test_pmem_merged,
              "Flush single merged descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
