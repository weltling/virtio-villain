/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0054: vsock_event_buf_too_short
 *
 * Post a writable descriptor on the event queue (queue 2) with a
 * length smaller than sizeof(virtio_vsock_event). Spec 5.10.6.6
 * says the device must not write past the buffer. The device must
 * either reject the entry or write at most the available bytes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_event_short(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *guard = vv_alloc_pages(1);
    memset(guard, 0xCC, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(guard), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    /* Bytes 1 and beyond must remain 0xCC */
    for (int i = 1; i < 64; i++) {
        if (guard[i] != 0xCC)
            TFAIL("guard[i] != 0xCC");
    }
    return r;
}

REGISTER_TEST_Q(V0054, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_event_short,
                "Event queue buffer shorter than event struct",
                VIRTIO_SPEC_V1_2, "5.10.6.6", 2);
