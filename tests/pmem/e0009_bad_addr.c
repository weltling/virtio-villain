/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0009: Pmem flush bad request addr.
 *
 * Request descriptor addr=0xFFFFFFFFFFFF0000, outside any mapping.
 * Device must refuse to dereference.
 *
 * Spec 5.19.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_pmem_bad_addr(struct virtio_dev *dev,
                                        struct vring *vr)
{
    void *resp = vv_alloc_pages(1);
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       sizeof(struct virtio_pmem_resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0009, VIRTIO_PCI_DEVICE_PMEM, test_pmem_bad_addr,
              "Flush bad request address",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
