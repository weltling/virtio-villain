/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0205: virtio-blk keeps serving I/O across a guest memory hot-add.
 *
 * The sidecar grows guest RAM through resize --memory after the
 * banner appears, which hot-adds a region to the guest memory map
 * while the queue is busy. This is a robustness check on the block
 * device's submit and complete paths during a memory map change, in
 * the same family as the pause, resume and snapshot sidecars. A grow
 * leaves the existing low RAM regions in place, so it is not a
 * reproducer for any one bug; it guards the resize plus I/O path
 * against regressions. The guest keeps a batch of reads in flight
 * across the resize window and asserts every one completes with
 * status OK and the device stays alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

/* The harness allocates a 16 entry ring; 5 chains use 15 descriptors. */
#define BATCH 5

struct read_slot {
    struct virtio_blk_outhdr *hdr;
    uint8_t *data;
    uint8_t *st;
};

static void setup_slots(struct vring *vr, struct read_slot *slots)
{
    for (int j = 0; j < BATCH; j++) {
        slots[j].hdr = vv_alloc_pages(1);
        slots[j].data = vv_alloc_pages(1);
        slots[j].st = vv_alloc_pages(1);
        slots[j].hdr->type = VIRTIO_BLK_T_IN;
        slots[j].hdr->ioprio = 0;
        slots[j].hdr->sector = (uint64_t)j;

        uint16_t base = (uint16_t)(j * 3);
        vring_raw_set_desc(vr, base, vv_virt_to_phys(slots[j].hdr),
                           sizeof(*slots[j].hdr),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(slots[j].data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(slots[j].st), 1,
                           VRING_DESC_F_WRITE, 0);
    }
}

static test_result_t do_batch(struct virtio_dev *dev, struct vring *vr,
                              struct read_slot *slots)
{
    for (int j = 0; j < BATCH; j++)
        *slots[j].st = 0xFF;

    uint16_t idx = vr->avail->idx;
    for (int j = 0; j < BATCH; j++)
        vring_raw_set_avail(vr, (uint16_t)((idx + j) % vr->size),
                            (uint16_t)(j * 3));
    vring_raw_set_avail_idx(vr, (uint16_t)(idx + BATCH));

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, BATCH, VV_TIMEOUT_MS * 4);
    if (r != TEST_PASS)
        return r;

    for (int j = 0; j < BATCH; j++)
        if (*slots[j].st != 0)
            return TEST_FAIL;
    return TEST_PASS;
}

static test_result_t test_blk_resize_memory_during_io(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    if (vr->size < BATCH * 3)
        return TEST_SKIP;

    struct read_slot slots[BATCH];
    setup_slots(vr, slots);

    /*
     * Sustain in-flight reads long enough for the sidecar resize to
     * land while requests are outstanding. Each iteration submits a
     * fresh batch and drains it, so the queue stays busy across the
     * memory hot-add.
     */
    for (int i = 0; i < 200; i++) {
        test_result_t r = do_batch(dev, vr, slots);
        if (r != TEST_PASS)
            return r;
        usleep(20000); /* 20ms */
    }
    return TEST_PASS;
}

REGISTER_TEST(B0205, VIRTIO_PCI_DEVICE_BLK, test_blk_resize_memory_during_io,
              "virtio-blk survives a guest memory hot-add mid I/O",
              VIRTIO_SPEC_V1_2, "2.6");
