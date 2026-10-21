/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0096: Normal initialization accepts offered ACCESS_PLATFORM (spec 2.2).
 *
 * An IOMMU-backed device may require ACCESS_PLATFORM before accepting
 * FEATURES_OK, even when the caller requests no optional features.
 */
#include "tests/test.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_access_platform_negotiation(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ACCESS_PLATFORM))
        return TEST_SKIP;

    if (virtio_pci_init(dev) < 0)
        TFAIL("Default initialization rejected with ACCESS_PLATFORM offered");

    if (virtio_pci_init_features(dev,
            (unsigned __int128)1 << VIRTIO_F_VERSION_1) < 0)
        TFAIL("Requested features rejected with ACCESS_PLATFORM offered");

    return TEST_PASS;
}

REGISTER_TEST(S0096, VIRTIO_PCI_DEVICE_BLK, test_access_platform_negotiation,
              "Initialization accepts offered ACCESS_PLATFORM",
              VIRTIO_SPEC_V1_2, "2.2");
