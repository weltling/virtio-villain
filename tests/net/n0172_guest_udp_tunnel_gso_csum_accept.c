/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0172: net_guest_udp_tunnel_gso_csum_accept
 *
 * Negotiate VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO_CSUM (bit 66) with its
 * required dependencies and confirm the device accepts it, reaching
 * FEATURES_OK and DRIVER_OK without setting NEEDS_RESET. Spec 5.1.3
 * feature bit requirements: GUEST_UDP_TUNNEL_GSO_CSUM requires
 * GUEST_UDP_TUNNEL_GSO, which requires the guest TSO and USO bits and
 * GUEST_CSUM. Skips when the device does not offer the feature, as CH
 * does not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_guest_udp_tunnel_gso_csum_accept(
    struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev,
                                    VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO_CSUM))
        return TEST_SKIP;

    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after GUEST_UDP_TUNNEL_GSO_CSUM "
              "negotiation (status 0x%02x)", status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after GUEST_UDP_TUNNEL_GSO_CSUM "
              "negotiation (status 0x%02x)", status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after GUEST_UDP_TUNNEL_GSO_CSUM "
              "negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0172, VIRTIO_PCI_DEVICE_NET,
                       test_net_guest_udp_tunnel_gso_csum_accept,
                       "device accepts GUEST_UDP_TUNNEL_GSO_CSUM negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO_CSUM) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_TSO4) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_TSO6) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_USO4) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_USO6) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_GUEST_CSUM), 0);
