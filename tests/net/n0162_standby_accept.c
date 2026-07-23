/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0162: net_standby_accept
 *
 * With VIRTIO_NET_F_STANDBY negotiated, confirm the device accepted
 * the feature and stayed healthy. STANDBY (spec 5.1.3) is a pure
 * signaling bit with no config field or dedicated queue: it tells the
 * driver the net device is a standby for a primary device with the
 * same MAC address. A driver SHOULD negotiate it when offered, so the
 * device must accept the negotiation (FEATURES_OK then DRIVER_OK) and
 * must not flag NEEDS_RESET.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_standby_accept(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_STANDBY))
        return TEST_SKIP;

    /* The harness negotiated STANDBY as a required feature, so the
     * device must have accepted it and reached DRIVER_OK. */
    uint8_t status = dev->common->device_status;

    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK clear after STANDBY negotiation (status 0x%02x)",
              status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after STANDBY negotiation (status 0x%02x)",
              status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after STANDBY negotiation");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0162, VIRTIO_PCI_DEVICE_NET, test_net_standby_accept,
                       "device accepts STANDBY feature negotiation",
                       VIRTIO_SPEC_V1_4, "5.1.3",
                       (1ULL << VIRTIO_NET_F_STANDBY), 0);
