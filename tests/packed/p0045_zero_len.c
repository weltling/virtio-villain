/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0045: Packed descriptor with zero length buffer.
 *
 * Spec 2.8.4: Present a packed descriptor with len=0 and the
 * WRITE flag set. The device must handle the zero length buffer
 * without crashing or infinite looping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_zero_len(struct virtio_dev *dev,
                                          struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(buf), 0, 0,
                          VRING_PACKED_DESC_F_WRITE);
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0045, VIRTIO_PCI_DEVICE_BLK, test_packed_zero_len,
                     "Packed descriptor with zero length buffer",
                     VIRTIO_SPEC_V1_2, "2.8.4");
