/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0140: TX two frames in one avail batch.
 *
 * Submit two valid minimal Ethernet frames in a single avail ring
 * update with one kick. Both must complete. Tests that the device
 * processes multiple TX buffers per notification.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_batch(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_net_hdr *h1 = vv_alloc_pages(1);
    uint8_t *f1 = vv_alloc_pages(1);
    struct virtio_net_hdr *h2 = vv_alloc_pages(1);
    uint8_t *f2 = vv_alloc_pages(1);

    /* Frame 1 */
    h1->flags = 0; h1->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    h1->hdr_len = 0; h1->gso_size = 0;
    h1->csum_start = 0; h1->csum_offset = 0;
    memset(f1, 0xFF, 6); memset(f1 + 6, 0x02, 6);
    f1[12] = 0x08; f1[13] = 0x00;
    memset(f1 + 14, 0x01, 46);

    /* Frame 2 */
    h2->flags = 0; h2->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    h2->hdr_len = 0; h2->gso_size = 0;
    h2->csum_start = 0; h2->csum_offset = 0;
    memset(f2, 0xFF, 6); memset(f2 + 6, 0x03, 6);
    f2[12] = 0x08; f2[13] = 0x00;
    memset(f2 + 14, 0x02, 46);

    /* Chain 1: descs 0,1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h1), sizeof(*h1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(f1), 60, 0, 0);

    /* Chain 2: descs 2,3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(h2), sizeof(*h2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(f2), 60, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0140, VIRTIO_PCI_DEVICE_NET, test_net_tx_batch,
              "TX two frames in one avail batch",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
