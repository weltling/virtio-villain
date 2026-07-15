/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0025: request a full page (4096 bytes) of entropy.
 *
 * Submit a 4096 byte writable buffer and verify the device fills
 * it (at least some bytes are non zero). Tests that the RNG device
 * handles larger entropy requests beyond the minimum.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_4k(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 4096,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    int nonzero = 0;
    for (int i = 0; i < 4096; i++) {
        if (buf[i] != 0) { nonzero = 1; break; }
    }
    if (!nonzero)
        TFAIL("4096 bytes of entropy are all zero");

    return TEST_PASS;
}

REGISTER_TEST(RNG0025, VIRTIO_PCI_DEVICE_RNG, test_rng_4k,
              "Request full page (4096 bytes) of entropy",
              VIRTIO_SPEC_V1_2, "5.4.6");
