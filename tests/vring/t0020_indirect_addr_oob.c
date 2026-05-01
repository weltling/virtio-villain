/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0020: indirect_addr_oob
 *
 * Set the indirect descriptor's address to a location beyond guest RAM.
 * The VMM must translate this GPA to read the indirect table; if it
 * doesn't bounds-check, it may read from arbitrary host memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_indirect_addr_oob(struct virtio_dev *dev,
                                            struct vring *vr)
{
    /*
     * Main descriptor: INDIRECT pointing to an address way beyond
     * any reasonable guest RAM.
     */
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL,
                       3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0020, VIRTIO_PCI_DEVICE_BLK, test_indirect_addr_oob,
              "Indirect table address beyond guest RAM",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
