/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0123: Kick with no descriptors in avail ring.
 *
 * Notify the device with an empty available ring. The device must
 * handle this gracefully without processing any requests.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_blk_empty_avail_kick(struct virtio_dev *dev,
                                               struct vring *vr)
{
    /* avail_idx stays at 0 — no descriptors posted */
    vring_raw_set_avail_idx(vr, 0);
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(200000);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(B0123, VIRTIO_PCI_DEVICE_BLK, test_blk_empty_avail_kick,
              "Kick with empty available ring",
              VIRTIO_SPEC_V1_2, "5.2.6");
