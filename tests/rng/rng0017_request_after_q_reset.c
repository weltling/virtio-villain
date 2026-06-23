/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0017: rng_request_after_queue_reset
 *
 * Reset the virtqueue via queue_enable=0, reallocate the vring,
 * re-enable, and submit a new request. Spec 2.6.1 says an
 * individual queue reset must leave the device operational.
 * The device must serve entropy after queue reset.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_after_q_reset(struct virtio_dev *dev,
                                            struct vring *vr)
{
    /* Submit an initial request */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Reset queue 0 via the queue_reset register (spec 2.6.1) */
    if (virtio_pci_queue_reset(dev, 0) < 0)
        return TEST_SKIP;

    /* Reallocate and re-enable */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);

    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf2, 0, 64);
    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(buf2), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0017, VIRTIO_PCI_DEVICE_RNG, test_rng_after_q_reset,
              "Entropy request after individual queue reset",
              VIRTIO_SPEC_V1_3, "2.6.1");
