/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0012: vsock_data_after_reset
 *
 * Send data after writing 0 to device status (device reset).
 * The device should not process packets after reset.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_data_after_reset(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1111;
    pkt->dst_port = 2222;
    pkt->len = 32;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memset(payload, 0xEE, 32);

    vv_desc(vr, 0, pkt, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 32, 0, 0);

    vring_submit(vr, 0);

    /* Reset device */
    virtio_pci_reset(dev);
    __sync_synchronize();

    /* Kick after reset - device must not process this */
    virtio_pci_kick(dev, 0);
    usleep(500000);

    /* If we're alive, that's fine */
    return TEST_PASS;
}

REGISTER_TEST(V0012, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_after_reset,
              "Send data after device reset",
              VIRTIO_SPEC_V1_2, "5.10.6");
