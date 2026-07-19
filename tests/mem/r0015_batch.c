/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0015: Virtio-mem batch plug requests.
 *
 * Submit several plug requests back-to-back across distinct
 * descriptor pairs to exercise multi-request handling.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_batch(struct virtio_dev *dev,
                                    struct vring *vr)
{
    struct virtio_mem_req *reqs = vv_alloc_pages(1);
    struct virtio_mem_resp *resps = vv_alloc_pages(1);
    uint64_t base = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);

    for (int i = 0; i < 4; i++) {
        memset(&reqs[i], 0, sizeof(reqs[i]));
        reqs[i].type = VIRTIO_MEM_REQ_STATE;
        reqs[i].addr = base + (uint64_t)i * 0x10000000ULL;
        reqs[i].nb_blocks = 1;

        uint64_t rp = vv_virt_to_phys(&reqs[i]);
        uint64_t sp = vv_virt_to_phys(&resps[i]);
        vring_raw_set_desc(vr, i * 2, rp, sizeof(reqs[i]),
                           VRING_DESC_F_NEXT, i * 2 + 1);
        vring_raw_set_desc(vr, i * 2 + 1, sp, sizeof(resps[i]),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i * 2);
    }
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0015, VIRTIO_PCI_DEVICE_MEM, test_mem_batch,
              "Batch state query requests",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
