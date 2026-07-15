/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0024: two consecutive requests both return non zero entropy.
 *
 * Submit two 32 byte requests back to back and verify both
 * contain at least one non zero byte. Tests that the RNG device
 * refills its entropy pool between consecutive requests.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_two_verify(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf1, 0, 32);
    memset(buf2, 0, 32);

    /* Request 1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf1), 32,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Request 2 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf2), 32,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Verify both have non zero content */
    int nz1 = 0, nz2 = 0;
    for (int i = 0; i < 32; i++) {
        if (buf1[i] != 0) nz1 = 1;
        if (buf2[i] != 0) nz2 = 1;
    }

    if (!nz1) TFAIL("first request returned all zeros");
    if (!nz2) TFAIL("second request returned all zeros");

    return TEST_PASS;
}

REGISTER_TEST(RNG0024, VIRTIO_PCI_DEVICE_RNG, test_rng_two_verify,
              "Two consecutive requests both return non zero entropy",
              VIRTIO_SPEC_V1_2, "5.4.6");
