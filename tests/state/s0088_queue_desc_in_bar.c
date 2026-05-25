/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0088: queue_desc (descriptor table base) at device MMIO BAR.
 *
 * Spec 4.1.4.3 and 2.7: queue_desc names the guest physical
 * address of the descriptor table itself. Program queue_desc
 * to point at the device's own common configuration BAR, then
 * kick the queue. A device that fetches descriptors through
 * the generic memory API without validating the source region
 * can read its own register layout as descriptors and act on
 * the resulting garbage addr, len, and flags fields. The
 * device must reject the impossible descriptor table base or
 * stay safely idle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_state_queue_desc_in_bar(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint64_t mmio_phys = dev->common_phys;

    cfg->queue_select = vr->queue;
    __sync_synchronize();
    cfg->queue_desc = mmio_phys;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(S0088, VIRTIO_PCI_DEVICE_BLK,
              test_state_queue_desc_in_bar,
              "queue_desc pointing at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
