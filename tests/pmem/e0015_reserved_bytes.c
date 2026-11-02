/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0015: Pmem flush reserved request bytes.
 *
 * Allocate a 32-byte request region with bytes 4..31 set to 0xAA.
 * Only the first 4 bytes are defined; the device must ignore any
 * trailing data and still process the flush.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_reserved_bytes(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 256);
    *(uint32_t *)buf = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 32, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0015, VIRTIO_PCI_DEVICE_PMEM, test_pmem_reserved_bytes,
              "Flush oversized request",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
