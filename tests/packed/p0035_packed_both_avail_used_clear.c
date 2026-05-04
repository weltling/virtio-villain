/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0035: Submit descriptor with both AVAIL and USED bits cleared.
 *
 * Spec 2.8.1: A descriptor is available when its AVAIL bit matches
 * the wrap counter and USED does not. With both bits clear, the
 * descriptor is neither available nor used - an invalid state for
 * submission. The device must handle this gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_both_clear(struct virtio_dev *dev,
                                            struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    uint16_t idx = vr->next_avail;

    /*
     * Use raw set to force both AVAIL and USED bits to 0.
     * Regardless of wrap_counter, this is an invalid descriptor state.
     */
    vring_packed_raw_set_desc(vr, idx, hdr_phys, sizeof(*hdr), 0,
                              VRING_PACKED_DESC_F_NEXT);
    vring_packed_raw_set_desc(vr, idx + 1, data_phys, 512, 0,
                              VRING_PACKED_DESC_F_NEXT |
                              VRING_PACKED_DESC_F_WRITE);
    vring_packed_raw_set_desc(vr, idx + 2, status_phys, 1, 0,
                              VRING_PACKED_DESC_F_WRITE);
    __sync_synchronize();

    /* Kick without proper AVAIL/USED flags */
    virtio_pci_kick(dev, vr->queue);

    /* Wait and see if device processes it or stays silent */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        uint16_t flags = vr->desc[idx].flags;
        int avail = !!(flags & VRING_PACKED_DESC_F_AVAIL);
        int used = !!(flags & VRING_PACKED_DESC_F_USED);
        if (avail == used)
            if (avail != 0) /* device marked it used */
                return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0035, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_both_clear,
                     "Packed descriptor with both AVAIL and USED clear",
                     VIRTIO_SPEC_V1_2, "2.8.1");
