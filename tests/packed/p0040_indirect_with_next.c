/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0040: packed_indirect_with_next
 *
 * Set both INDIRECT and NEXT on the same packed descriptor.
 * Spec 2.8.6 forbids the combination. The device must reject or
 * stay silent rather than chasing both paths.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_indirect_next(struct virtio_dev *dev,
                                               struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(buf), 4096, 0,
                          VRING_PACKED_DESC_F_INDIRECT
                          | VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, vr->wrap_counter,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0040, VIRTIO_PCI_DEVICE_BLK, test_packed_indirect_next,
                     "INDIRECT and NEXT set on the same packed descriptor",
                     VIRTIO_SPEC_V1_2, "2.8.6");
