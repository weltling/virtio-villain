/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0178: net_ipsec_security_rx_absent
 *
 * Without negotiating VIRTIO_NET_F_IPSEC, post a receive buffer and
 * confirm the device does not report IPsec security state on incoming
 * packets. Spec 5.1.6.4: the device sets VIRTIO_NET_HDR_F_SECURITY
 * only when a packet goes through IPsec processing, which cannot occur
 * unless VIRTIO_NET_F_IPSEC is negotiated, so the SECURITY, SECURITY_ERR
 * and SA soft-expiry warning bits MUST stay clear here.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_ipsec_security_rx_absent(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf),
                       sizeof(struct virtio_net_hdr_mrg) + 1500,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    volatile struct virtio_net_hdr_mrg *hdr =
        (volatile struct virtio_net_hdr_mrg *)buf;
    uint8_t sec = VIRTIO_NET_HDR_F_SECURITY |
                  VIRTIO_NET_HDR_F_SECURITY_ERR |
                  VIRTIO_NET_HDR_F_SECURITY_SA_SOFT_EXPIRY_WARN;
    if (hdr->flags & sec)
        TFAIL("IPsec security flag set without IPSEC negotiated "
              "(flags 0x%02x)", hdr->flags);

    return TEST_PASS;
}

REGISTER_TEST(N0178, VIRTIO_PCI_DEVICE_NET, test_net_ipsec_security_rx_absent,
              "no IPsec security flags on RX without the feature",
              VIRTIO_SPEC_V1_4, "5.1.6.4");
