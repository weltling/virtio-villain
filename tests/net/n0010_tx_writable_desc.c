/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0010: net_tx_writable_desc
 *
 * Place a device-writable descriptor in the TX queue. Spec 5.1.6.4.2:
 * driver MUST NOT place device-writable buffers into the tx queue.
 * The device must reject rather than reading from a write-only buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_writable_desc(struct virtio_dev *dev,
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

    /* TX descriptors with WRITE flag - wrong direction */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0010, VIRTIO_PCI_DEVICE_NET, test_net_tx_writable_desc,
              "Device-writable descriptor in TX queue",
              VIRTIO_SPEC_V1_2, "5.1.6.4");
