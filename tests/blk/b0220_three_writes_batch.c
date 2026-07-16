/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0220: three writes in one avail batch.
 *
 * Submit three WRITE requests (sectors 0, 1, 2) in a single avail
 * ring update with one kick. All three must complete with S_OK.
 * Tests deeper batch write processing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_three_writes(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdrs[3];
    uint8_t *datas[3];
    uint8_t *sts[3];

    for (int i = 0; i < 3; i++) {
        hdrs[i] = vv_alloc_pages(1);
        datas[i] = vv_alloc_pages(1);
        sts[i] = vv_alloc_pages(1);
        hdrs[i]->type = VIRTIO_BLK_T_OUT;
        hdrs[i]->ioprio = 0;
        hdrs[i]->sector = i;
        memset(datas[i], (uint8_t)(0x30 + i), 512);
        *sts[i] = 0xFF;

        int base = i * 3;
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdrs[i]),
                           sizeof(*hdrs[i]), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(datas[i]), 512,
                           VRING_DESC_F_NEXT, base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(sts[i]), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, base);
    }
    vring_raw_set_avail_idx(vr, 3);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    for (int i = 0; i < 3; i++)
        if (*sts[i] != VIRTIO_BLK_S_OK)
            TFAIL("write %d status %u", i, *sts[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0220, VIRTIO_PCI_DEVICE_BLK, test_blk_three_writes,
              "Three writes in one batch all return S_OK",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
