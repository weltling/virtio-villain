/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0008: Pmem flush huge response length.
 *
 * Response descriptor len=1 GiB but backed by one page. Device
 * must clamp or reject without overrunning the page.
 *
 * Spec 5.19.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_pmem_huge_resp(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    void *r = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(r), 1u << 30,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0008, VIRTIO_PCI_DEVICE_PMEM, test_pmem_huge_resp,
              "Flush huge response length",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
