/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0027: request 8 bytes of entropy.
 *
 * Submit an 8 byte writable buffer. Tests the device handles
 * small requests correctly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_8(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 8);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 8,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    int nz = 0;
    for (int i = 0; i < 8; i++)
        if (buf[i] != 0) { nz = 1; break; }
    if (!nz) TFAIL("8 bytes all zero");

    return TEST_PASS;
}

REGISTER_TEST(RNG0027, VIRTIO_PCI_DEVICE_RNG, test_rng_8,
              "Request 8 bytes of entropy",
              VIRTIO_SPEC_V1_2, "5.4.6");
