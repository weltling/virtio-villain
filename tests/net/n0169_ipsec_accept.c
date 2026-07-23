/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0169: net_ipsec_accept
 *
 * Negotiate VIRTIO_NET_F_IPSEC (bit 70) and confirm the device
 * accepts it, reaching FEATURES_OK and DRIVER_OK without setting
 * NEEDS_RESET. Spec 5.1.3: IPSEC indicates the device supports
 * inline IPsec processing. Skips when the device does not offer the
 * feature, as CH does not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_ipsec_accept(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_IPSEC))
        return TEST_SKIP;

    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after IPSEC negotiation "
              "(status 0x%02x)", status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after IPSEC negotiation "
              "(status 0x%02x)", status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after IPSEC negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0169, VIRTIO_PCI_DEVICE_NET, test_net_ipsec_accept,
                       "device accepts IPSEC negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       VV_FEATURE_BIT(VIRTIO_NET_F_IPSEC), 0);
