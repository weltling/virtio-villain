/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0023: verify entropy response contains non zero bytes.
 *
 * Submit a 64 byte request to the RNG device and verify that
 * at least some bytes in the response are non zero. A device
 * returning all zeros for 64 bytes of entropy is statistically
 * implausible and likely indicates the device failed to fill.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_nonzero(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    int nonzero = 0;
    for (int i = 0; i < 64; i++) {
        if (buf[i] != 0) {
            nonzero = 1;
            break;
        }
    }

    if (!nonzero)
        TFAIL("64 bytes of entropy are all zero");

    return TEST_PASS;
}

REGISTER_TEST(RNG0023, VIRTIO_PCI_DEVICE_RNG, test_rng_nonzero,
              "Entropy response contains non zero bytes",
              VIRTIO_SPEC_V1_2, "5.4.6");
