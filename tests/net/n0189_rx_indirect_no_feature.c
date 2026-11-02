/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0189: rx_indirect_no_feature
 *
 * A receive descriptor sets VRING_DESC_F_INDIRECT without having
 * negotiated VIRTIO_F_INDIRECT_DESC. Spec 2.7.5.3 forbids the flag
 * unless the feature was negotiated, so the device must refuse it
 * without harm to the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_indirect_no_feature(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    indirect[0].addr = vv_virt_to_phys(buf);
    indirect[0].len = 1526;
    indirect[0].flags = VRING_DESC_F_WRITE;
    indirect[0].next = 0;

    vv_desc(vr, 0, indirect, sizeof(struct vring_desc), VRING_DESC_F_INDIRECT,
            0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0189, VIRTIO_PCI_DEVICE_NET, test_net_rx_indirect_no_feature,
                "Receive indirect flag without the feature",
                VIRTIO_SPEC_V1_2, "2.7.5.3", 0);
