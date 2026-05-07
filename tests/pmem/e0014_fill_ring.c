/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0014: Pmem flush fill ring.
 *
 * Post 16 flush requests (one per slot, two-descriptor chains
 * each) into the ring and advance avail->idx in one shot.
 *
 * Spec 5.10.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_pmem_req { uint32_t type; } __attribute__((packed));
struct virtio_pmem_resp { uint32_t ret; } __attribute__((packed));

static test_result_t test_pmem_fill_ring(struct virtio_dev *dev,
                                         struct vring *vr)
{
    /*
     * vr->size is 16; we have 16 descriptor slots. Pack the chain
     * heads at even slots and continuations at odd slots: 8
     * requests, two slots per request.
     */
    struct virtio_pmem_req *reqs = vv_alloc_pages(1);
    struct virtio_pmem_resp *resps = vv_alloc_pages(1);
    uint16_t pairs = vr->size / 2;

    for (uint16_t i = 0; i < pairs; i++) {
        reqs[i].type = 0;
        vring_raw_set_desc(vr, i * 2,
                           vv_virt_to_phys(&reqs[i]), sizeof(reqs[i]),
                           VRING_DESC_F_NEXT, i * 2 + 1);
        vring_raw_set_desc(vr, i * 2 + 1,
                           vv_virt_to_phys(&resps[i]), sizeof(resps[i]),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i * 2);
    }
    vring_raw_set_avail_idx(vr, pairs);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0014, VIRTIO_PCI_DEVICE_PMEM, test_pmem_fill_ring,
              "Flush fill the ring",
              VIRTIO_SPEC_V1_2, "5.10.6.1");
