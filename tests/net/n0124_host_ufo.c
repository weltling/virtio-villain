/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0124: HOST_UFO advertisement requires HOST_CSUM.
 *
 * v1.4 5.1.3: VIRTIO_NET_F_HOST_UFO (bit 14) only makes sense
 * if the device can also offload checksumming. Verify HOST_UFO,
 * if offered, comes with CSUM (bit 0).
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t lo = cfg->device_feature;
    if (!(lo & (1U << VIRTIO_NET_F_HOST_UFO)))
        return TEST_SKIP;
    if (!(lo & (1U << VIRTIO_NET_F_CSUM)))
        TFAIL("HOST_UFO offered without CSUM");
    return TEST_PASS;
}

REGISTER_TEST(N0124, VIRTIO_PCI_DEVICE_NET, test,
              "HOST_UFO paired with CSUM",
              VIRTIO_SPEC_V1_4, "5.1.3");
