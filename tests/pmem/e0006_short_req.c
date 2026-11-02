/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0006: Pmem flush short request.
 *
 * Request descriptor of 1 byte, smaller than uint32_t type.
 * Device must reject the malformed input.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_short_req(struct virtio_dev *dev,
                                         struct vring *vr)
{
    void *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 1, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 64,
                       sizeof(struct virtio_pmem_resp),
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0006, VIRTIO_PCI_DEVICE_PMEM, test_pmem_short_req,
              "Flush short request",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
