/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0034: net_tx_wrong_queue_index
 *
 * Submit a TX frame on a queue index that would be above the configured
 * number of queue pairs. With 1 pair, queues 0=RX, 1=TX. We try to use
 * the vring on queue 1 (TX) but make the descriptor layout look like
 * an RX buffer (writable). Tests device's per-queue direction checking.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_wrong_queue(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /*
     * Abuse the TX queue by posting a fully writable descriptor
     * (as if it were an RX buffer). The device should not fill a
     * TX-queue buffer with received data.
     */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xCC, 1500);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Post writable buffer on TX queue (wrong direction) */
    vring_raw_set_desc(vr, 0, buf_phys, 1500, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0034, VIRTIO_PCI_DEVICE_NET, test_net_tx_wrong_queue,
              "TX queue with writable-only descriptor (wrong direction)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
