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
#include "lib/virtio_test.h"

/* virtio-blk request header */
static test_result_t test_desc_next_oob(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t qsz = vr->size;

    /*
     * Descriptor chain:
     *   [0] header -> next = queue_size (OOB!)
     *
     * Slot 0 is valid but its next index exceeds the table boundary.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, qsz);

    /* Put a plausible descriptor just past the table boundary in case
     * the VMM reads it without crashing (we want to distinguish
     * "silently read garbage" from "properly rejected"). */
    (void)vv_virt_to_phys(data);

    /* Submit head 0 */
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0003, VIRTIO_PCI_DEVICE_BLK, test_desc_next_oob,
              "Descriptor next index out of bounds",
              VIRTIO_SPEC_V1_2, "2.7.5");
