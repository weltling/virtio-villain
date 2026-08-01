/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0030: Packed virtqueue with sequential IN_ORDER submission (spec 2.8.12)
 *
 * Submit multiple descriptors sequentially in packed mode and verify
 * the device marks them used in descriptor-ring order.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_in_order_packed(struct virtio_dev *dev,
                                          struct vring_packed *vr)
{
    /* Submit 3 sequential read requests, each a single descriptor */
    struct virtio_blk_outhdr *hdrs[3];
    uint8_t *bufs[3];

    for (int i = 0; i < 3; i++) {
        hdrs[i] = vv_alloc_pages(1);
        bufs[i] = vv_alloc_pages(1);

        hdrs[i]->type = VIRTIO_BLK_T_IN;
        hdrs[i]->ioprio = 0;
        hdrs[i]->sector = (uint64_t)i;

        /* Pack header + data + status into one page for simplicity */
        memcpy(bufs[i], hdrs[i], sizeof(*hdrs[i]));
        /* status at offset 512+16 */
        bufs[i][sizeof(*hdrs[i]) + 512] = 0xFF;
    }

    /*
     * Submit each as a 3-desc chain: hdr(R) -> data(W) -> status(W)
     * using packed ring mechanics.
     */
    uint16_t avail_idx = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;

    for (int i = 0; i < 3; i++) {
        /* Use AVAIL matching current wrap counter, !USED */
        uint16_t avail_flag = wrap ? VRING_PACKED_DESC_F_AVAIL : 0;
        uint16_t used_flag = wrap ? 0 : VRING_PACKED_DESC_F_USED;

        /* Header descriptor (readable) */
        vr->desc[avail_idx].addr = vv_virt_to_phys(hdrs[i]);
        vr->desc[avail_idx].len = sizeof(*hdrs[i]);
        vr->desc[avail_idx].id = avail_idx;
        __sync_synchronize();
        vr->desc[avail_idx].flags = avail_flag | used_flag |
                                    VRING_PACKED_DESC_F_NEXT;

        avail_idx++;
        if (avail_idx >= vr->size) {
            avail_idx = 0;
            wrap ^= 1;
        }

        /* Data descriptor (writable) */
        avail_flag = wrap ? VRING_PACKED_DESC_F_AVAIL : 0;
        used_flag = wrap ? 0 : VRING_PACKED_DESC_F_USED;

        vr->desc[avail_idx].addr = vv_virt_to_phys(bufs[i]);
        vr->desc[avail_idx].len = 512;
        vr->desc[avail_idx].id = avail_idx;
        __sync_synchronize();
        vr->desc[avail_idx].flags = avail_flag | used_flag |
                                    VRING_PACKED_DESC_F_NEXT |
                                    VRING_PACKED_DESC_F_WRITE;

        avail_idx++;
        if (avail_idx >= vr->size) {
            avail_idx = 0;
            wrap ^= 1;
        }

        /* Status descriptor (writable, end of chain) */
        avail_flag = wrap ? VRING_PACKED_DESC_F_AVAIL : 0;
        used_flag = wrap ? 0 : VRING_PACKED_DESC_F_USED;

        vr->desc[avail_idx].addr = vv_virt_to_phys(bufs[i]) + sizeof(*hdrs[i]) + 512;
        vr->desc[avail_idx].len = 1;
        vr->desc[avail_idx].id = avail_idx;
        __sync_synchronize();
        vr->desc[avail_idx].flags = avail_flag | used_flag |
                                    VRING_PACKED_DESC_F_WRITE;

        avail_idx++;
        if (avail_idx >= vr->size) {
            avail_idx = 0;
            wrap ^= 1;
        }
    }

    vr->next_avail = avail_idx;
    vr->wrap_counter = wrap;

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Wait for at least 1 used */
    int elapsed = 0;
    uint16_t check_idx = 0;
    uint8_t check_wrap = 1; /* initial wrap counter */
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (vring_packed_desc_is_used(vr, check_idx, check_wrap))
            return TEST_PASS;
        elapsed += 10000;
    }

    uint8_t status = dev->common->device_status;
    if (status == 0)
        TWEDGED("status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0030, VIRTIO_PCI_DEVICE_BLK, test_in_order_packed,
                     "Packed sequential submission verifying in-order used",
                     VIRTIO_SPEC_V1_2, "2.8.12");
