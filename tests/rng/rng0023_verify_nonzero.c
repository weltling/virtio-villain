/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0023: verify entropy response contains non zero bytes.
 *
 * Submit a 64 byte request to the RNG device and verify that
 * at least some bytes in the response are non zero. A device
 * returning all zeros for 64 bytes of entropy is statistically
 * implausible and likely indicates the device failed to fill.
 */
#include "lib/virtio_test.h"

static test_result_t test_rng_nonzero(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 64, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

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
