/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0082: Descriptor with all fields at maximum values (spec 2.7.5)
 *
 * Set addr=UINT64_MAX, len=UINT32_MAX, flags=0xFFFF, next=UINT16_MAX
 * on a descriptor. The device must handle extreme values gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_max_values(struct virtio_dev *dev,
                                          struct vring *vr)
{
    /* Set descriptor 0 with all maximum values */
    vr->desc[0].addr = 0xFFFFFFFFFFFFFFFFULL;
    vr->desc[0].len = 0xFFFFFFFF;
    vr->desc[0].flags = 0xFFFF;
    vr->desc[0].next = 0xFFFF;

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0082, VIRTIO_PCI_DEVICE_BLK, test_desc_max_values,
              "Descriptor with addr/len/flags/next all at UINT_MAX",
              VIRTIO_SPEC_V1_2, "2.7.5");
