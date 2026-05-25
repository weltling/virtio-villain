/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0089: queue_avail (avail ring base) at device MMIO BAR.
 *
 * Spec 4.1.4.3 and 2.7: queue_avail names the guest physical
 * address of the avail ring. Program queue_avail to point at
 * the device's own common configuration BAR, then kick. A
 * device that reads the avail idx and ring entries through
 * the generic memory API without validating the source region
 * can interpret its own register layout as avail entries and
 * dispatch garbage descriptor indices. The device must reject
 * the impossible avail ring base or stay safely idle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_state_queue_avail_in_bar(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint64_t mmio_phys = dev->common_phys;

    cfg->queue_select = vr->queue;
    __sync_synchronize();
    cfg->queue_avail = mmio_phys;
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(S0089, VIRTIO_PCI_DEVICE_BLK,
              test_state_queue_avail_in_bar,
              "queue_avail pointing at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
