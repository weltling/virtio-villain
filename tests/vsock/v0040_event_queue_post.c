/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0040: Event queue buffer posting (spec 5.10.6.7)
 *
 * Post a buffer on the vsock event queue (queue 2) and verify the
 * device handles it. The event queue is used for transport events
 * like connection state changes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_event_queue(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* vsock needs at least 3 queues: RX(0), TX(1), event(2) */
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* Set up event queue (queue 2) */
    struct vring evvr;
    vring_alloc(&evvr, 16);
    vring_attach(dev, &evvr, 2);

    /* Post a writable buffer for events */
    struct virtio_vsock_event *evt = vv_alloc_pages(1);
    memset(evt, 0xFF, sizeof(*evt));

    vring_raw_set_desc(&evvr, 0, vv_virt_to_phys(evt), sizeof(*evt),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&evvr, 0, 0);
    vring_raw_set_avail_idx(&evvr, 1);

    /* Kick event queue */
    test_result_t r = vv_kick_and_wait(dev, &evvr, 2, VV_TIMEOUT_MS);

    /* Event queue might not produce events without activity - REJECT is fine */
    return r;
}

REGISTER_TEST(V0040, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_event_queue,
              "Post buffer on vsock event queue (queue 2)",
              VIRTIO_SPEC_V1_2, "5.10.6.7");
