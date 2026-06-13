/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0013: net_tx_invalid_queue_idx
 *
 * Queue a packet on a TX queue index greater than the negotiated
 * number of queue pairs minus one. Spec 5.1.6.5.6.1: driver MUST NOT
 * queue packets on transmit queues greater than virtqueue_pairs.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_invalid_queue_idx(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    memset(frame, 0x42, 64);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /*
     * Kick queue index 99 - far beyond what any net device with
     * default single queue pair would have configured.
     */
    return vv_kick_and_wait(dev, vr, 99, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0013, VIRTIO_PCI_DEVICE_NET, test_net_tx_invalid_queue_idx,
              "Kick invalid queue index beyond pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
