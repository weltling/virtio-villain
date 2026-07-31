/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0134: feature_change
 *
 * The device offers the reported change feature so the driver can rely
 * on parameter change events for the attached logical units.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_feat_change(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_CHANGE))
        TFAIL("reported change feature not offered");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0134, VIRTIO_PCI_DEVICE_SCSI, test_scsi_feat_change,
              "Device offers the reported change feature",
              VIRTIO_SPEC_V1_4, "5.6.3");
