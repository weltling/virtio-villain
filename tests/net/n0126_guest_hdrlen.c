/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0126: GUEST_HDRLEN feature offered.
 *
 * v1.4 5.1.3 plus VIRTIO_NET_F_GUEST_HDRLEN (bit 59): when
 * negotiated, the device can tell the driver the size of the
 * header it expects via a config field. Verify the feature
 * advertisement does not imply a config layout the driver
 * cannot read.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << (VIRTIO_NET_F_GUEST_HDRLEN - 32))))
        return TEST_SKIP;
    if (!dev->device_cfg || dev->device_cfg_length < 12)
        TFAIL("GUEST_HDRLEN offered but device config too small");
    return TEST_PASS;
}

REGISTER_TEST(N0126, VIRTIO_PCI_DEVICE_NET, test,
              "GUEST_HDRLEN feature backed by usable config",
              VIRTIO_SPEC_V1_4, "5.1.3");
