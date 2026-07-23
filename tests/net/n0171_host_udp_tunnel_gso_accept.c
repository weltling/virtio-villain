/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0171: net_host_udp_tunnel_gso_accept
 *
 * Negotiate VIRTIO_NET_F_HOST_UDP_TUNNEL_GSO (bit 67) with its
 * required dependencies and confirm the device accepts it, reaching
 * FEATURES_OK and DRIVER_OK without setting NEEDS_RESET. Spec 5.1.3
 * feature bit requirements: HOST_UDP_TUNNEL_GSO requires HOST_TSO4
 * and HOST_TSO6, which in turn require CSUM. Skips when the device
 * does not offer the feature, as CH does not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_host_udp_tunnel_gso_accept(
    struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_HOST_UDP_TUNNEL_GSO))
        return TEST_SKIP;

    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after HOST_UDP_TUNNEL_GSO negotiation "
              "(status 0x%02x)", status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after HOST_UDP_TUNNEL_GSO negotiation "
              "(status 0x%02x)", status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after HOST_UDP_TUNNEL_GSO "
              "negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0171, VIRTIO_PCI_DEVICE_NET,
                       test_net_host_udp_tunnel_gso_accept,
                       "device accepts HOST_UDP_TUNNEL_GSO negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       VV_FEATURE_BIT(VIRTIO_NET_F_HOST_UDP_TUNNEL_GSO) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_HOST_TSO4) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_HOST_TSO6) |
                       VV_FEATURE_BIT(VIRTIO_NET_F_CSUM), 0);
