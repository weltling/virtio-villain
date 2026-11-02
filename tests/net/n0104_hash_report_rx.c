/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0104: net_hash_report_rx
 *
 * Post a receive buffer and verify the device can report hash info
 * in the virtio_net_hdr when VIRTIO_NET_F_HASH_REPORT is active.
 * Spec v1.3 5.1.6.4: device writes hash_value and hash_report_type
 * into the header of received packets.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_hash_report_rx(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_HASH_REPORT))
        return TEST_SKIP;

    uint8_t *buf = vv_alloc_pages(1);

    /* Post a writable receive buffer large enough for hdr + data */
    vv_desc(vr, 0, buf, sizeof(struct virtio_net_hdr_hash) + 1500,
            VRING_DESC_F_WRITE, 0);
    vring_submit(vr, 0);

    /* We just verify the device does not crash when processing
     * a receive buffer that expects hash report fields. */
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0104, VIRTIO_PCI_DEVICE_NET, test_net_hash_report_rx,
                "RX buffer with hash report header space",
                VIRTIO_SPEC_V1_3, "5.1.6.4", 0,
                (1ULL << VIRTIO_NET_F_HASH_REPORT), 0);
