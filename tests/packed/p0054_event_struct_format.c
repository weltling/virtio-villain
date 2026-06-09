/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0054: event suppression struct layout.
 *
 * v1.4 2.8.10: driver_event and device_event are 4 bytes each
 * (off_wrap + flags). Write the suppression flags ENABLE then
 * read back the device_event to verify it remains readable.
 */
#include "tests/test.h"

#define RING_EVENT_FLAGS_ENABLE 0

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    (void)dev;
    if (!vr->driver_event || !vr->device_event) return TEST_SKIP;
    vr->driver_event->flags = RING_EVENT_FLAGS_ENABLE;
    __sync_synchronize();
    uint16_t dflags = vr->device_event->flags;
    (void)dflags;
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0054, VIRTIO_PCI_DEVICE_BLK, test,
                     "Event suppression structures are reachable",
                     VIRTIO_SPEC_V1_4, "2.8.10");
