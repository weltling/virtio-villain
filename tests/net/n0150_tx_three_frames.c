/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0150: TX three frames in one batch.
 *
 * Submit three valid Ethernet frames in one avail ring update.
 * All three must complete. Tests deeper batch processing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_triple(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_net_hdr *hdrs[3];
    uint8_t *frames[3];

    for (int i = 0; i < 3; i++) {
        hdrs[i] = vv_alloc_pages(1);
        frames[i] = vv_alloc_pages(1);
        hdrs[i]->flags = 0;
        hdrs[i]->gso_type = VIRTIO_NET_HDR_GSO_NONE;
        hdrs[i]->hdr_len = 0; hdrs[i]->gso_size = 0;
        hdrs[i]->csum_start = 0; hdrs[i]->csum_offset = 0;
        memset(frames[i], 0xFF, 6);
        memset(frames[i] + 6, (uint8_t)(0x02 + i), 6);
        frames[i][12] = 0x08; frames[i][13] = 0x00;
        memset(frames[i] + 14, (uint8_t)i, 46);
    }

    for (int i = 0; i < 3; i++) {
        int base = i * 2;
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdrs[i]),
                           sizeof(*hdrs[i]), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(frames[i]),
                           60, 0, 0);
        vring_raw_set_avail(vr, i, base);
    }
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0150, VIRTIO_PCI_DEVICE_NET, test_net_tx_triple,
              "TX three frames in one batch",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
