/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0028: two requests in one avail batch.
 *
 * Submit two 16 byte RNG buffers in one avail ring update with
 * a single kick. Both must complete. Tests batch entropy request
 * handling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_batch(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf1, 0, 16);
    memset(buf2, 0, 16);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf1), 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf2), 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0028, VIRTIO_PCI_DEVICE_RNG, test_rng_batch,
              "Two requests in one avail batch",
              VIRTIO_SPEC_V1_2, "5.4.6");
