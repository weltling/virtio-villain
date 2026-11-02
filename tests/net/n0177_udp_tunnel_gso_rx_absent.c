/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0177: net_udp_tunnel_gso_rx_absent
 *
 * Without negotiating VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO, post a
 * receive buffer and confirm the device does not report UDP tunnel
 * segmentation on incoming packets. Spec 5.1.6.4: if
 * VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO is not negotiated the device MUST
 * NOT set the VIRTIO_NET_HDR_GSO_UDP_TUNNEL_IPV4 or _IPV6 bits in
 * gso_type, and if VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO_CSUM is not
 * negotiated it MUST NOT set the VIRTIO_NET_HDR_F_UDP_TUNNEL_CSUM bit
 * in flags. Neither feature is negotiated here.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_udp_tunnel_gso_rx_absent(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, sizeof(struct virtio_net_hdr_mrg) + 1500,
            VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    volatile struct virtio_net_hdr_mrg *hdr =
        (volatile struct virtio_net_hdr_mrg *)buf;
    if (hdr->gso_type & (VIRTIO_NET_HDR_GSO_UDP_TUNNEL_IPV4 |
                         VIRTIO_NET_HDR_GSO_UDP_TUNNEL_IPV6))
        TFAIL("UDP tunnel gso_type bit set without GUEST_UDP_TUNNEL_GSO "
              "(gso_type 0x%02x)", hdr->gso_type);
    if (hdr->flags & VIRTIO_NET_HDR_F_UDP_TUNNEL_CSUM)
        TFAIL("UDP_TUNNEL_CSUM flag set without GUEST_UDP_TUNNEL_GSO_CSUM "
              "(flags 0x%02x)", hdr->flags);

    return TEST_PASS;
}

REGISTER_TEST(N0177, VIRTIO_PCI_DEVICE_NET, test_net_udp_tunnel_gso_rx_absent,
              "no UDP tunnel gso on RX without the feature",
              VIRTIO_SPEC_V1_4, "5.1.6.4");
