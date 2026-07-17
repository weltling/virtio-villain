/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0117: EVENT_IDX basic read of used_event field.
 *
 * Spec 2.7.7.1: When VIRTIO_F_EVENT_IDX is negotiated, the driver
 * writes used_event at the end of the avail ring to suppress
 * notifications until the used ring reaches that index. Write
 * used_event = 1 (suppress until one completion), submit a read,
 * verify completion occurs. This exercises the EVENT_IDX path
 * without checking interrupt suppression (no IRQ handler).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_event_idx(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_F_EVENT_IDX)))
        return TEST_SKIP;

    /* Set used_event to 1 (at the end of the avail ring) */
    uint16_t qsize = vr->size;
    uint16_t *used_event_p =
        (uint16_t *)((char *)vr->avail + 4 + qsize * 2);
    *used_event_p = 1;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("status %u with EVENT_IDX active", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(T0117, VIRTIO_PCI_DEVICE_BLK, test_event_idx,
              "EVENT_IDX used_event set then request completes",
              VIRTIO_SPEC_V1_2, "2.7.7.1",
              (1ULL << VIRTIO_F_EVENT_IDX), 0);
