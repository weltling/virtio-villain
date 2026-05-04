/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0072: net_rss_indirection_overflow
 *
 * Submit RSS config with indirection_table_mask pointing beyond table
 * size. Tests that the device rejects invalid RSS configuration.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t cmd;
} __attribute__((packed));

/*
 * RSS config structure (simplified).
 * The indirection table mask must be <= actual table entries - 1.
 */
struct virtio_net_rss_config {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint16_t indirection_table[1]; /* variable length */
    uint16_t max_tx_vq;
    uint8_t  hash_key_length;
    uint8_t  hash_key_data[40];
} __attribute__((packed));

#define VIRTIO_NET_CTRL_MQ         4
#define VIRTIO_NET_CTRL_MQ_RSS_CONFIG 1

static test_result_t test_net_rss_overflow(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Need ctrl queue */
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    struct vring ctrlvr;
    vring_alloc(&ctrlvr, 16);
    vring_attach(dev, &ctrlvr, 2);

    /* Ctrl header */
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->cmd = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;

    /* RSS config with oversized mask */
    struct virtio_net_rss_config *rss = vv_alloc_pages(1);
    memset(rss, 0, sizeof(*rss));
    rss->hash_types = 0x1; /* some hash type */
    rss->indirection_table_mask = 0xFFFF; /* way beyond table size */
    rss->unclassified_queue = 0;
    rss->indirection_table[0] = 0;
    rss->max_tx_vq = 1;
    rss->hash_key_length = 40;
    memset(rss->hash_key_data, 0x6d, 40);

    uint8_t *ack = vv_alloc_pages(1);
    *ack = 0xFF;

    vring_raw_set_desc(&ctrlvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrlvr, 1, vv_virt_to_phys(rss), sizeof(*rss),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrlvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&ctrlvr, 0, 0);
    vring_raw_set_avail_idx(&ctrlvr, 1);

    return vv_kick_and_wait(dev, &ctrlvr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0072, VIRTIO_PCI_DEVICE_NET, test_net_rss_overflow,
              "RSS indirection_table_mask beyond table size",
              VIRTIO_SPEC_V1_2, "5.1.6.5.8");
