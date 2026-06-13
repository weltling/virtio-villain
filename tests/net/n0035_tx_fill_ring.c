/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0035: net_tx_fill_ring
 *
 * Fill the entire TX available ring with minimal frames. Tests
 * that the device processes a full ring without stalling or
 * corrupting the used ring index.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_fill_ring(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint16_t qsz = vr->size;
    /* Each frame needs 2 descriptors: hdr + payload */
    uint16_t max_frames = qsz / 2;

    struct virtio_net_hdr *hdrs = vv_alloc_pages(4);
    uint8_t *frames = vv_alloc_pages(4);

    for (uint16_t i = 0; i < max_frames; i++) {
        uint16_t base = i * 2;
        struct virtio_net_hdr *h = &hdrs[i];
        memset(h, 0, sizeof(*h));

        uint8_t *f = frames + (i * 64);
        memset(f, 0x42, 64);

        vring_raw_set_desc(vr, base, vv_virt_to_phys(h), sizeof(*h),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(f), 64, 0, 0);

        vring_raw_set_avail(vr, i, base);
    }

    vring_raw_set_avail_idx(vr, max_frames);

    return vv_kick_and_wait(dev, vr, 0, 1000);
}

REGISTER_TEST(N0035, VIRTIO_PCI_DEVICE_NET, test_net_tx_fill_ring,
              "Fill entire TX avail ring with frames",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
