/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0039: device_event reads do not crash device
 *
 * Spec 2.8.10 makes device_event a read only field for the driver
 * (it is updated by the device when VIRTIO_F_RING_EVENT_IDX is
 * negotiated). Read off_wrap and flags repeatedly with no driver
 * activity and confirm the values are stable enough not to flap
 * between every read, and that the device stays alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_packed_device_event_read(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t f0, w0;

    f0 = vr->device_event->flags;
    w0 = vr->device_event->off_wrap;
    __sync_synchronize();

    int flap_flags = 0;
    int flap_wrap = 0;
    for (int i = 0; i < 5000; i++) {
        if (vr->device_event->flags != f0)
            flap_flags++;
        if (vr->device_event->off_wrap != w0)
            flap_wrap++;
    }

    if (cfg->device_status == 0)
        TFAIL("cfg->device_status == 0");

    /* Some flap is allowed but pure noise on every read is bad */
    if (flap_flags > 100 || flap_wrap > 100)
        TFAIL("flap_flags > 100 || flap_wrap > 100");

    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0039, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_device_event_read,
                     "device_event reads stay mostly stable when idle",
                     VIRTIO_SPEC_V1_2, "2.8.10");
