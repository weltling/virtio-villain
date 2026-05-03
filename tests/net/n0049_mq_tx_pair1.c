/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0049: net_multiqueue_tx_nonzero_pair
 *
 * Send a TX packet on queue pair > 0 (queue index 2 for TX, since
 * pairs are RX0/TX0=0/1, RX1/TX1=2/3, etc.). Tests device
 * multiqueue transmit path beyond the default pair.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

#define VIRTIO_NET_F_MQ 22

static test_result_t test_net_mq_tx_pair1(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Need at least 4 queues for pair 1 (queues 0,1,2,3) + ctrl */
    if (cfg->num_queues < 5)
        return TEST_SKIP;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_NET_F_MQ)))
        return TEST_SKIP;

    /* TX queue for pair 1 is queue index 3 (RX1=2, TX1=3) */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 3);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    /* Simple ethernet frame */
    memset(frame, 0xFF, 6);       /* dst broadcast */
    memset(frame + 6, 0x02, 6);   /* src */
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x55, 46); /* minimum payload */

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0049, VIRTIO_PCI_DEVICE_NET, test_net_mq_tx_pair1,
              "Multiqueue TX on queue pair 1 (non-default pair)",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
