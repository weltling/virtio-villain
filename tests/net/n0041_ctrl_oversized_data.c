/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0041: net_ctrl_oversized_data
 *
 * Submit a control queue command with a data buffer much larger than
 * expected for the command type. Tests whether the device properly
 * bounds-checks the data length or blindly reads the full descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_oversized_data(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1); /* 4096 bytes - way oversized */
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    /* PROMISC expects 1 byte of data (0 or 1). We send 4096 bytes. */
    memset(data, 0x01, 4096);
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 4096, /* oversized! */
                       VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0041, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_oversized_data,
              "Control command with oversized data buffer (4096 bytes)",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
