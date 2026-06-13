/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0010: Virtio-mem missing response descriptor.
 *
 * Submit a plug request without a writable response chain. The
 * device cannot return its response and must reject the chain
 * rather than crash.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_no_resp(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0010, VIRTIO_PCI_DEVICE_MEM, test_mem_no_resp,
              "Plug without response descriptor",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
