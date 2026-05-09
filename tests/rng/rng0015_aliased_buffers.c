/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0015: rng_aliased_buffers
 *
 * Submit eight writable descriptor chains in the same kick where
 * every descriptor points at the same one page buffer. Spec 5.4.6
 * lets the device complete each chain with however many bytes it
 * wants, but the device must not write past the buffer or hang.
 * The test passes if at least one chain is consumed without
 * wedging the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_aliased(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 64);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    for (int i = 0; i < 8; i++) {
        vring_raw_set_desc(vr, (uint16_t)i, buf_phys, 64,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, (uint16_t)i);
    }
    vring_raw_set_avail_idx(vr, 8);

    return vv_kick_and_wait_n(dev, vr, 0, 8, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0015, VIRTIO_PCI_DEVICE_RNG, test_rng_aliased,
              "Eight aliased entropy buffers in one kick",
              VIRTIO_SPEC_V1_2, "5.4.6");
