/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0096: ring filled with two batches of half qsize
 *
 * Spec 2.7.13 says the device must process every entry made
 * available between two reads of avail->idx. Submit half the
 * queue depth, wait for completions, then submit another half.
 * Both batches must complete in order. This catches devices
 * that lose track of available entries across notification
 * boundaries.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static int submit_batch(struct virtio_dev *dev, struct vring *vr,
                        uint16_t base_slot, uint16_t avail_base,
                        int count)
{
    for (int i = 0; i < count; i++) {
        struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *st = vv_alloc_pages(1);
        hdr->type = VIRTIO_BLK_T_IN;
        hdr->ioprio = 0;
        hdr->sector = i;
        *st = 0xFF;

        uint16_t s = base_slot + i * 3;
        vring_raw_set_desc(vr, s, vv_virt_to_phys(hdr), sizeof(*hdr),
                           VRING_DESC_F_NEXT, s + 1);
        vring_raw_set_desc(vr, s + 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, s + 2);
        vring_raw_set_desc(vr, s + 2, vv_virt_to_phys(st), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (avail_base + i) % vr->size, s);
    }
    vring_raw_set_avail_idx(vr, avail_base + count);

    uint16_t target = avail_base + count;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int tries = VV_TIMEOUT_MS;
    while (tries-- > 0) {
        __sync_synchronize();
        if (vr->used->idx == target)
            return 0;
        usleep(1000);
    }
    return -1;
}

static test_result_t test_two_batches(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint16_t qsize = vr->size;
    /* Use small batches that fit comfortably in qsize. Each entry
     * needs 3 desc slots, and the queue is shared with the avail
     * ring so keep the batch well below qsize / 3. */
    int half = qsize / 8;
    if (half < 2)
        return TEST_SKIP;

    if (submit_batch(dev, vr, 0, 0, half) < 0)
        TFAIL("submit_batch(dev, vr, 0, 0, half) < 0");
    if (submit_batch(dev, vr, half * 3, half, half) < 0)
        TFAIL("submit_batch(dev, vr, half * 3, half, half) < 0");
    return TEST_PASS;
}

REGISTER_TEST(T0096, VIRTIO_PCI_DEVICE_BLK, test_two_batches,
              "two sequential batches of half queue depth complete",
              VIRTIO_SPEC_V1_2, "2.7.13");
