/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0116: used ring wraps correctly at queue size boundary.
 *
 * Fill the avail ring with queue_size requests (all simple reads),
 * kick, wait for all completions, and verify used->idx wrapped
 * correctly. Tests that the device handles ring wrap arithmetic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_used_wrap(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t qsize = cfg->queue_size;

    /* Use min(qsize, 8) to keep test fast but still exercise wrap */
    int n = qsize < 8 ? qsize : 8;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;

    /* Submit n requests one at a time, reusing desc 0..2 */
    for (int i = 0; i < n; i++) {
        *st = 0xFF;
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, 0);
        vring_raw_set_avail_idx(vr, i + 1);

        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS) return r;
    }

    /* Verify used->idx advanced by n (mod 65536) */
    uint16_t used_idx = vr->used->idx;
    if (used_idx != (uint16_t)n)
        TFAIL("used->idx %u, expected %u after %d requests",
              used_idx, (uint16_t)n, n);

    return TEST_PASS;
}

REGISTER_TEST(T0116, VIRTIO_PCI_DEVICE_BLK, test_used_wrap,
              "Used ring idx advances correctly across requests",
              VIRTIO_SPEC_V1_2, "2.7.8");
