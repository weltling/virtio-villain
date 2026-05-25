/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0021: Three concurrent entropy reads in one batch.
 *
 * Spec 5.4.6: Push three independent writable descriptors in the
 * same avail batch. The device must populate each buffer
 * independently without mixing or skipping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_three_inflight(struct virtio_dev *dev,
                                             struct vring *vr)
{
    for (int i = 0; i < 3; i++) {
        uint8_t *buf = vv_alloc_pages(1);
        memset(buf, 0, 16);

        vring_raw_set_desc(vr, (uint16_t)i, vv_virt_to_phys(buf), 16,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, (uint16_t)i);
    }
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0021, VIRTIO_PCI_DEVICE_RNG, test_rng_three_inflight,
              "Three concurrent entropy reads in one batch",
              VIRTIO_SPEC_V1_2, "5.4.6");
