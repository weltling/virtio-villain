/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0055: notif_suppression_overflow
 *
 * Set avail_event (used_event in the available ring) to a value that
 * would cause a wrapping overflow when compared to avail.idx.
 * A VMM using naive signed comparison may miscalculate whether to
 * send a notification.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_notif_suppression_overflow(struct virtio_dev *dev,
                                                     struct vring *vr)
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /*
     * Set used_event (at avail->ring[qsz]) to 0xFFFE.
     * Then set avail.idx to 1. The wrapping difference is huge,
     * which may confuse notification suppression logic.
     */
    uint16_t qsz = vr->size;
    volatile uint16_t *used_event =
        (volatile uint16_t *)((char *)vr->avail + 4 + qsz * 2);
    *used_event = 0xFFFE;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0055, VIRTIO_PCI_DEVICE_BLK, test_notif_suppression_overflow,
              "Notification suppression with wrapping event idx",
              VIRTIO_SPEC_V1_2, "2.7.13");
