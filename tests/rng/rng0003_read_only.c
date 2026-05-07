/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0003: RNG read-only descriptor.
 *
 * Submit a requestq descriptor without VRING_DESC_F_WRITE. Per
 * spec 5.4.6.1 the driver MUST NOT place device-readable buffers
 * into the queue. The device must reject the descriptor rather
 * than write into a read-only mapping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_read_only(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0003, VIRTIO_PCI_DEVICE_RNG, test_rng_read_only,
              "RNG read-only descriptor",
              VIRTIO_SPEC_V1_2, "5.4.6.1");
