/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0072: indirect_zero_valid_entries
 *
 * Set up an indirect descriptor table with len indicating entries
 * but the first entry has addr=0 and len=0. The indirect table
 * technically exists but contains no useful data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_indirect_zero_valid(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /*
     * Allocate an indirect table of 3 entries (48 bytes) but fill
     * all entries with zeroed addr/len. The device should either
     * reject (addr=0 is invalid) or not crash parsing them.
     */
    struct vring_desc *indirect = vv_alloc_pages(1);
    memset(indirect, 0, 48); /* 3 x 16-byte descs, all zeros */

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Point main descriptor at the indirect table with INDIRECT flag */
    vring_raw_set_desc(vr, 0, indirect_phys, 48,
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0072, VIRTIO_PCI_DEVICE_BLK, test_indirect_zero_valid,
              "Indirect table with all-zero descriptor entries",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
