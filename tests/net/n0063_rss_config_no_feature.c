/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0063: CTRL_MQ RSS_CONFIG without VIRTIO_NET_F_RSS
 *
 * Spec 5.1.6.5.7.2: "A driver MUST NOT send the
 * VIRTIO_NET_CTRL_MQ_RSS_CONFIG command if the feature
 * VIRTIO_NET_F_RSS has not been negotiated."
 *
 * Send RSS_CONFIG without negotiating VIRTIO_NET_F_RSS.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VIRTIO_NET_CTRL_MQ              4
#define VIRTIO_NET_CTRL_MQ_RSS_CONFIG   1

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

/* Minimal RSS config structure (just enough to trigger validation) */
struct virtio_net_rss_config {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint16_t indirection_table[1];
    uint16_t max_tx_vq;
    uint8_t  hash_key_length;
    uint8_t  hash_key_data[40];
} __attribute__((packed));

static test_result_t test_rss_config_no_feature(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_rss_config *rss = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;

    memset(rss, 0, sizeof(*rss));
    rss->hash_types = 0x1; /* some hash type */
    rss->indirection_table_mask = 0;
    rss->unclassified_queue = 0;
    rss->indirection_table[0] = 0;
    rss->max_tx_vq = 0;
    rss->hash_key_length = 40;
    memset(rss->hash_key_data, 0xAB, 40);

    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(rss), sizeof(*rss),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0063, VIRTIO_PCI_DEVICE_NET, test_rss_config_no_feature,
              "CTRL_MQ RSS_CONFIG without VIRTIO_NET_F_RSS negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.5.7");
