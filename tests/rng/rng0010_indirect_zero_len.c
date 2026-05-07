/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0010: RNG indirect with zero length.
 *
 * Submit an indirect requestq descriptor whose len field is 0,
 * implying zero entries. Spec 2.7.7 requires len to be a non-zero
 * multiple of 16. The device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_indirect_zero_len(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);
    uint64_t itab_phys = vv_virt_to_phys(itab);

    vring_raw_set_desc(vr, 0, itab_phys, 0,
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0010, VIRTIO_PCI_DEVICE_RNG, test_rng_indirect_zero_len,
              "RNG indirect with zero length",
              VIRTIO_SPEC_V1_2, "2.7.7");
