/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0028: net_ctrl_empty_data
 *
 * Issue a control queue command with only the header and ack byte but
 * no data buffer between them. Some commands require a data payload;
 * the device must handle a missing/empty data gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_empty_data(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /*
     * CTRL_RX PROMISC normally expects a 1-byte on/off data payload.
     * We skip it entirely - just header + ack.
     */
    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *status = 0xFF;

    /* descriptor 0: header only (device-readable) */
    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    /* descriptor 1: ack byte (device-writable) - no data in between */
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0028, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_empty_data,
              "Control command with no data payload",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
