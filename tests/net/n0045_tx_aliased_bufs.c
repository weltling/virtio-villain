/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0045: net_tx_aliased_header_data
 *
 * Submit a TX packet where the virtio-net header descriptor and the
 * frame data descriptor point to the same physical memory. Tests
 * whether the device handles aliased buffers (where reading the frame
 * would also see header bytes and vice versa).
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
} __attribute__((packed));

#define VIRTIO_NET_HDR_GSO_NONE 0

static test_result_t test_net_tx_aliased_bufs(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Write header at start of buffer */
    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)buf;
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Frame data also starts at the same address */
    memset(buf + sizeof(*hdr), 0xFF, 60);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Both descriptors point to same physical page */
    vring_raw_set_desc(vr, 0, buf_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys, 60 + sizeof(*hdr), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0045, VIRTIO_PCI_DEVICE_NET, test_net_tx_aliased_bufs,
              "TX with header and data descriptors aliasing same memory",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
