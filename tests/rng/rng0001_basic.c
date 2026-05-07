/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0001: RNG basic request.
 *
 * Submit a single writable descriptor to the entropy device's
 * requestq (queue 0). Per spec 5.4.6 the device must place at
 * least one random byte into the buffer and advance the used
 * ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_basic(struct virtio_dev *dev,
                                    struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 64, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0001, VIRTIO_PCI_DEVICE_RNG, test_rng_basic,
              "RNG basic request",
              VIRTIO_SPEC_V1_2, "5.4.6");
