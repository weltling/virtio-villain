/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0174: net_guest_uso6_accept
 *
 * Negotiate the guest USO receive feature VIRTIO_NET_F_GUEST_USO6
 * (bit 55) together with its required VIRTIO_NET_F_GUEST_CSUM and
 * confirm the device accepts it. Spec 5.1.3 lists the guest USO
 * feature bits, and per the feature bit requirements GUEST_USO6
 * requires GUEST_CSUM. A device that offers GUEST_USO6 must accept
 * the negotiation, reaching FEATURES_OK and DRIVER_OK without
 * setting NEEDS_RESET. Only n0096 covered the negative path, a USO
 * gso_type without the feature; this is the positive acceptance for
 * the IPv6 variant, the companion to n0163 for GUEST_USO4.
 * Skips when the device does not offer GUEST_USO6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_guest_uso6_accept(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_GUEST_USO6))
        return TEST_SKIP;

    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after GUEST_USO6 negotiation "
              "(status 0x%02x)", status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after GUEST_USO6 negotiation "
              "(status 0x%02x)", status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after GUEST_USO6 negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0174, VIRTIO_PCI_DEVICE_NET, test_net_guest_uso6_accept,
                       "device accepts GUEST_USO6 negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       (1ULL << VIRTIO_NET_F_GUEST_USO6) |
                       (1ULL << VIRTIO_NET_F_GUEST_CSUM), 0);
