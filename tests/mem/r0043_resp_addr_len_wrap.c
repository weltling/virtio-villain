/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0043: virtio_mem response writable addr plus len wraps 2^64.
 *
 * Siblings R0024 keeps the response end address below 2^64, and R0017
 * and R0033 cover the request field arithmetic. Here the response
 * descriptor base sits near the top of the address space and the length
 * makes addr plus len wrap to a low value, so a device that computes the
 * end with a plain addition gets a small wrapped result and a naive
 * bounds check passes. The device must not access memory outside the
 * guest mapping or crash the VMM. Completing, silently rejecting, or
 * wedging the queue are all acceptable.
 *
 * Spec 5.15.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_resp_addr_len_wrap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = 0;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0043, VIRTIO_PCI_DEVICE_MEM, test_mem_resp_addr_len_wrap,
              "Mem response writable addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "5.15.6");
