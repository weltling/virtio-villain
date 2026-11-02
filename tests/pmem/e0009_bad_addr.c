/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0009: Pmem flush bad request addr.
 *
 * Request descriptor addr=0xFFFFFFFFFFFF0000, outside any mapping.
 * Device must refuse to dereference.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_bad_addr(struct virtio_dev *dev,
                                        struct vring *vr)
{
    void *resp = vv_alloc_pages(1);
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 4,
                       VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(struct virtio_pmem_resp), VRING_DESC_F_WRITE,
            0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0009, VIRTIO_PCI_DEVICE_PMEM, test_pmem_bad_addr,
              "Flush bad request address",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
