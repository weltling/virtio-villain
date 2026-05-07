/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0013: RNG with NEXT|WRITE|INDIRECT flags.
 *
 * Submit a requestq descriptor with NEXT, WRITE, and INDIRECT
 * flags simultaneously. Spec 2.7.5.3.1 says NEXT and INDIRECT are
 * mutually exclusive. The device must reject this combination.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_all_flags(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE |
                       VRING_DESC_F_INDIRECT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + 16, 16, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0013, VIRTIO_PCI_DEVICE_RNG, test_rng_all_flags,
              "RNG with NEXT|WRITE|INDIRECT flags",
              VIRTIO_SPEC_V1_2, "2.7.5.3.1");
