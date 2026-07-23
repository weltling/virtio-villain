/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0168: net_out_net_header_accept
 *
 * Negotiate VIRTIO_NET_F_OUT_NET_HEADER (bit 69) and confirm the
 * device accepts it, reaching FEATURES_OK and DRIVER_OK without
 * setting NEEDS_RESET. Spec 5.1.3: OUT_NET_HEADER lets the driver
 * provide the start of the network header, and the feature has no
 * dependency on other net feature bits. This is the first net
 * feature at bit 64 or above to be negotiated, exercising the
 * widened 128-bit feature path. Skips when the device does not offer
 * OUT_NET_HEADER, as CH does not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_out_net_header_accept(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_OUT_NET_HEADER))
        return TEST_SKIP;

    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after OUT_NET_HEADER negotiation "
              "(status 0x%02x)", status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after OUT_NET_HEADER negotiation "
              "(status 0x%02x)", status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after OUT_NET_HEADER negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0168, VIRTIO_PCI_DEVICE_NET,
                       test_net_out_net_header_accept,
                       "device accepts OUT_NET_HEADER negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       VV_FEATURE_BIT(VIRTIO_NET_F_OUT_NET_HEADER), 0);
