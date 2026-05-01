/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0019: indirect_len_zero
 *
 * Set an indirect descriptor's len to 0. This means the indirect table
 * contains zero descriptors - there is no request to process.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_indirect_len_zero(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    uint64_t page_phys = vv_virt_to_phys(page);

    /*
     * Main descriptor: INDIRECT with len = 0 (empty table).
     */
    vring_raw_set_desc(vr, 0, page_phys, 0,
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0019, VIRTIO_PCI_DEVICE_BLK, test_indirect_len_zero,
              "Indirect descriptor with len = 0",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
