/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0021: Watchdog ping with packed virtqueue.
 *
 * Spec 2.8/5.20: Submit a watchdog ping using the packed ring
 * format. The device must accept the descriptor and complete
 * without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_packed_ping(struct virtio_dev *dev,
                                               struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 1);

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(buf), 1, 0,
                          VRING_PACKED_DESC_F_WRITE);
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(D0021, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_packed_ping,
                     "Watchdog ping via packed virtqueue",
                     VIRTIO_SPEC_V1_2, "5.20");
