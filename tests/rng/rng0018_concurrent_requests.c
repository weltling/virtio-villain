/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0018: rng_concurrent_requests
 *
 * Submit multiple entropy requests back to back by filling several
 * avail ring slots before kicking. Spec 2.7.7 says the device
 * must process each available descriptor. All buffers must be
 * returned via the used ring without corruption.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_concurrent(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *bufs[4];
    for (int i = 0; i < 4; i++) {
        bufs[i] = vv_alloc_pages(1);
        memset(bufs[i], 0, 64);
        vring_raw_set_desc(vr, (uint16_t)i,
                           vv_virt_to_phys(bufs[i]), 64,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, (uint16_t)i);
    }
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS * 2);
}

REGISTER_TEST(RNG0018, VIRTIO_PCI_DEVICE_RNG, test_rng_concurrent,
              "Four concurrent entropy requests in one kick",
              VIRTIO_SPEC_V1_2, "2.7.7");
