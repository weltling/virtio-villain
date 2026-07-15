/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0026: two requests return different data.
 *
 * Submit two 32 byte requests and verify they produce different
 * results. With 256 bits of entropy the probability of collision
 * is negligible. A device returning identical data twice likely
 * has a broken entropy source.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_different(struct virtio_dev *dev, struct vring *vr)
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

    if (memcmp(buf1, buf2, 32) == 0)
        TFAIL("two 32 byte requests returned identical data");

    return TEST_PASS;
}

REGISTER_TEST(RNG0026, VIRTIO_PCI_DEVICE_RNG, test_rng_different,
              "Two requests return different entropy",
              VIRTIO_SPEC_V1_2, "5.4.6");
