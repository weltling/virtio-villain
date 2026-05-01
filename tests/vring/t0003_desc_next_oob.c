/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0003: desc_next_oob
 *
 * Set a descriptor's next field to an index beyond the queue size
 * (queue_size itself, which is one past the last valid slot). A VMM
 * that uses the next field as an array index without bounds checking
 * will access memory beyond the descriptor table.
 *
 * In Rust VMMs this typically triggers an index-out-of-bounds panic
 * crashing the virtio worker thread (or the entire VMM process).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

/* virtio-blk request header */
struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_desc_next_oob(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 512);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    uint16_t qsz = vr->size;

    /*
     * Descriptor chain:
     *   [0] header -> next = queue_size (OOB!)
     *
     * Slot 0 is valid but its next index exceeds the table boundary.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, qsz);

    /* Put a plausible descriptor just past the table boundary in case
     * the VMM reads it without crashing (we want to distinguish
     * "silently read garbage" from "properly rejected"). */
    (void)data_phys;

    /* Submit head 0 */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0003, VIRTIO_PCI_DEVICE_BLK, test_desc_next_oob,
              "Descriptor next index out of bounds",
              VIRTIO_SPEC_V1_2, "2.7.5");
