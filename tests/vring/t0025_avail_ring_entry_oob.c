/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0025: avail_ring_entry_oob
 *
 * Place a descriptor index equal to queue_size in the available ring.
 * This is an out-of-bounds head index - it does not refer to any valid
 * slot in the descriptor table.
 *
 * A VMM that uses avail.ring[n] directly as an index into the
 * descriptor table without bounds checking will access out-of-bounds
 * memory. In Rust this panics; in C this is a heap/stack buffer
 * overread.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_avail_ring_entry_oob(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint16_t qsz = vr->size;

    /*
     * Write an invalid head index (queue_size) into avail.ring[0].
     * No valid descriptor chain exists - the index itself is illegal.
     */
    vring_raw_set_avail(vr, 0, qsz);
    vring_raw_set_avail_idx(vr, 1);

    /* Kick */
    (void)vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    /* Both consumed (error response) and dropped (silent reject) are safe */
    return TEST_PASS;
}

REGISTER_TEST(T0025, VIRTIO_PCI_DEVICE_BLK, test_avail_ring_entry_oob,
              "Available ring entry index out of bounds",
              VIRTIO_SPEC_V1_2, "2.7.6");
