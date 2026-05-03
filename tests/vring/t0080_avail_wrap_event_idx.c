/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0080: avail_ring_wrap_with_event_idx
 *
 * Advance the avail ring index close to UINT16_MAX, then wrap around.
 * With EVENT_IDX semantics, the used_event field at the end of the
 * avail ring might cause issues near the wrap boundary.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_avail_ring_wrap_event_idx(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /*
     * Set avail idx to just before UINT16_MAX to test wrap-around.
     * The desc slot (0) is always the same; we just advance avail.idx.
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Set avail idx to 0xFFFE, ring slot = 0xFFFE % queue_size */
    uint16_t start_idx = 0xFFFE;
    uint16_t slot = start_idx % vr->size;
    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, start_idx);

    /* Also set used_event to a value near wrap */
    vr->avail->ring[vr->size] = start_idx - 1;
    __sync_synchronize();

    /* Kick - device must handle the near-max idx */
    uint16_t before = vr->used->idx;
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before)
            goto first_done;
        elapsed += 10000;
    }
    TREJECT("timeout waiting for used at start_idx=avail_idx_max-1");

first_done:
    /* Now wrap: submit at idx 0xFFFF */
    *status = 0xFF;
    slot = 0xFFFF % vr->size;
    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, 0xFFFF);
    __sync_synchronize();

    before = vr->used->idx;
    virtio_pci_kick(dev, vr->queue);

    elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before)
            goto second_done;
        elapsed += 10000;
    }
    TREJECT("timeout waiting for used after avail_idx wrap to 0xFFFF");

second_done:
    /* Finally wrap to 0x0000 */
    *status = 0xFF;
    slot = 0x0000 % vr->size;
    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, 0x0000);
    __sync_synchronize();

    before = vr->used->idx;
    virtio_pci_kick(dev, vr->queue);

    elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0080, VIRTIO_PCI_DEVICE_BLK, test_avail_ring_wrap_event_idx,
              "Avail ring idx wrap at UINT16_MAX with used_event set",
              VIRTIO_SPEC_V1_2, "2.7.7");
