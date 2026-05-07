/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0005: Pmem flush with no response descriptor.
 *
 * Submit a flush req with only a readable descriptor, no
 * writable response chain. Per spec 5.10 the device cannot return
 * its 4-byte ret and must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_pmem_req { uint32_t type; } __attribute__((packed));

static test_result_t test_pmem_no_resp_chain(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0005, VIRTIO_PCI_DEVICE_PMEM, test_pmem_no_resp_chain,
              "Flush without response chain",
              VIRTIO_SPEC_V1_2, "5.10.6.1");
