/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0058: device_status writing reserved bit.
 *
 * Spec 2.1 defines bits 0,1,2,3,6,7. Writing bit 4 (value 0x10)
 * is reserved. The VMM must mask reserved bits or ignore them
 * without breaking state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_status_reserved(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    uint8_t s = dev->common->device_status;
    dev->common->device_status = (uint8_t)(s | 0x10);
    __sync_synchronize();
    usleep(2000);
    return TEST_PASS;
}

REGISTER_TEST(PCI0058, 0, test_pci_status_reserved,
              "device_status reserved bit set",
              VIRTIO_SPEC_V1_2, "2.1");
