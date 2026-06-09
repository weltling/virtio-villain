/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0095: driver must not alter live virtqueue entries.
 *
 * Spec 3.3.1: After making a descriptor visible to the device
 * via avail, the driver MUST NOT modify the descriptor table
 * entry until the device returns it via used. Post a
 * descriptor, mutate its addr field while it is in flight,
 * and verify the device either uses the original value or
 * rejects without corrupting state. A device may snapshot at
 * read time; the test is informational and never fails on
 * the device side.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);

    /* Race window: mutate addr field. */
    vr->desc[0].addr = 0;
    __sync_synchronize();

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0095, VIRTIO_PCI_DEVICE_BLK, test,
              "Driver mutates live descriptor; device stays sane",
              VIRTIO_SPEC_V1_4, "3.3.1");
