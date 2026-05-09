/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0020: pmem_status_with_dangling_next
 *
 * Submit a flush whose status descriptor has the NEXT flag set
 * but next points at a slot that is itself empty. Spec 2.7.5.2
 * forbids dangling NEXT chains. The device must reject the
 * request without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_pmem_req {
    uint32_t type;
} __attribute__((packed));

struct virtio_pmem_resp {
    uint32_t ret;
} __attribute__((packed));

#define VIRTIO_PMEM_REQ_TYPE_FLUSH 0

static test_result_t test_pmem_status_dangling(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 5);
    /* Slot 5 left as zero addr/len */

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0020, VIRTIO_PCI_DEVICE_PMEM, test_pmem_status_dangling,
              "Flush with status descriptor NEXT to empty slot",
              VIRTIO_SPEC_V1_2, "2.7.5.2");
