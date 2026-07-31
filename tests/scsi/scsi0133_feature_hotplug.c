/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0133: feature_hotplug
 *
 * The device offers the hot plug feature so the driver can rely on
 * transport reset events for logical units that appear or disappear.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_feat_hotplug(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_HOTPLUG))
        TFAIL("hot plug feature not offered");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0133, VIRTIO_PCI_DEVICE_SCSI, test_scsi_feat_hotplug,
              "Device offers the hot plug feature",
              VIRTIO_SPEC_V1_4, "5.6.3");
