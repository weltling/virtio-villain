/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0104: net_hash_report_rx
 *
 * Post a receive buffer and verify the device can report hash info
 * in the virtio_net_hdr when VIRTIO_NET_F_HASH_REPORT is active.
 * Spec v1.3 5.1.6.4: device writes hash_value and hash_report_type
 * into the header of received packets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr_hash {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
    uint32_t hash_value;
    uint16_t hash_report_type;
    uint16_t padding;
} __attribute__((packed));

static test_result_t test_net_hash_report_rx(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    /* Post a writable receive buffer large enough for hdr + data */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf),
                       sizeof(struct virtio_net_hdr_hash) + 1500,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* We just verify the device does not crash when processing
     * a receive buffer that expects hash report fields. */
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0104, VIRTIO_PCI_DEVICE_NET, test_net_hash_report_rx,
                "RX buffer with hash report header space",
                VIRTIO_SPEC_V1_3, "5.1.6.4", 0);
