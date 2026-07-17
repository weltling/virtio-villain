/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0131: ctrl_ack_zero_len
 *
 * Send a valid control command but with the ack descriptor having
 * zero length. Spec 5.1.6.5 says the device writes a 1-byte ack.
 * With zero length the device has nowhere to write the ack. It must
 * not write beyond the descriptor length and must not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_ack_zero_len(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *data = 1;
    *ack = 0xCC; /* canary: should stay unchanged */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    /* hdr (readable) -> data (readable) -> ack (writable, len=0) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, ack_phys, 0,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0131, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_ack_zero_len,
              "CTRL command with zero length ack descriptor",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
