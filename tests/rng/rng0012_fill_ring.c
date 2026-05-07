/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0012: RNG fill the entire ring.
 *
 * Post one writable descriptor in every slot of the requestq and
 * advance avail->idx to queue_size. The device must consume the
 * full batch and fill all buffers without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_fill_ring(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    for (uint16_t i = 0; i < vr->size; i++) {
        vring_raw_set_desc(vr, i, buf_phys + i * 16, 16,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, vr->size);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0012, VIRTIO_PCI_DEVICE_RNG, test_rng_fill_ring,
              "RNG fill the ring",
              VIRTIO_SPEC_V1_2, "5.4.6");
