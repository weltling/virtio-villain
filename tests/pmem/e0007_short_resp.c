/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0007: Pmem flush short response.
 *
 * Writable response of only 1 byte, less than the 4-byte
 * virtio_pmem_resp.ret. Device must reject.
 *
 * Spec 5.10.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_pmem_req { uint32_t type; } __attribute__((packed));

static test_result_t test_pmem_short_resp(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = 0;
    void *r = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(r), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0007, VIRTIO_PCI_DEVICE_PMEM, test_pmem_short_resp,
              "Flush short response",
              VIRTIO_SPEC_V1_2, "5.10.6.1");
