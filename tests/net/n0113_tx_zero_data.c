/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0113: Net TX with zero length data buffer.
 *
 * Spec 5.1.6: Submit a transmit request where the virtio_net_hdr
 * is followed by a zero length data descriptor. The device must
 * not crash on an empty frame body.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

static test_result_t test_net_tx_zero_data(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));

    /* Header descriptor (readable) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data descriptor with zero length */
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 0, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0113, VIRTIO_PCI_DEVICE_NET, test_net_tx_zero_data,
                "Net TX with zero length data buffer",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
