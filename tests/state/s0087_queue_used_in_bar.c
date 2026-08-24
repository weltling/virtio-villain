/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0087: queue_used (used ring base) programmed at device MMIO BAR.
 *
 * Spec 4.1.4.3 and 2.7: queue_used gives the guest physical
 * address of the used ring. The driver supplies normal RAM.
 * Program queue_used to point at the device's own common
 * configuration BAR, then kick. A device that writes used ring
 * entries through the generic memory API without validating
 * the target region can wedge or corrupt its own registers
 * when retiring a descriptor. The device must reject the
 * impossible used ring base or stay safely idle on the kick.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_state_queue_device_in_bar(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    uint64_t mmio_phys = dev->common_phys;

    cfg->queue_select = vr->queue;
    __sync_synchronize();

    virtio_store64(&cfg->queue_used, mmio_phys);
    __sync_synchronize();

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(vv_alloc_pages(1)), 8,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(S0087, VIRTIO_PCI_DEVICE_BLK,
              test_state_queue_device_in_bar,
              "queue_used pointing at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
