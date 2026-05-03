/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0064: RSS indirection table with non-power-of-2 mask
 *
 * Spec 5.1.6.5.7.2: "The number of entries in indirection_table
 * (indirection_table_mask + 1) MUST be a power of two."
 *
 * Send RSS_CONFIG with indirection_table_mask = 4 (5 entries, not
 * a power of 2).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VIRTIO_NET_CTRL_MQ              4
#define VIRTIO_NET_CTRL_MQ_RSS_CONFIG   1
#define VIRTIO_NET_F_RSS                60

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_rss_non_power_of_2(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t offered_hi = cfg->device_feature;
    if (!(offered_hi & (1U << (VIRTIO_NET_F_RSS - 32))))
        return TEST_SKIP;

    uint16_t nq = cfg->num_queues;
    if (nq < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *rss_buf = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;

    memset(rss_buf, 0, 4096);
    /* hash_types (u32) */
    uint32_t hash_types = 0x1;
    memcpy(rss_buf, &hash_types, 4);
    /* indirection_table_mask (u16) = 4 → 5 entries, NOT power of 2 */
    uint16_t mask = 4;
    memcpy(rss_buf + 4, &mask, 2);
    /* unclassified_queue (u16) */
    uint16_t unclass = 0;
    memcpy(rss_buf + 6, &unclass, 2);
    /* indirection_table: 5 entries of u16 */
    uint16_t table[5] = {0, 0, 0, 0, 0};
    memcpy(rss_buf + 8, table, 10);
    /* max_tx_vq (u16) */
    uint16_t max_tx = 0;
    memcpy(rss_buf + 18, &max_tx, 2);
    /* hash_key_length (u8) = 40 */
    rss_buf[20] = 40;
    /* hash_key_data: 40 bytes */
    memset(rss_buf + 21, 0xCD, 40);

    uint16_t rss_len = 21 + 40;
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(rss_buf), rss_len,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0064, VIRTIO_PCI_DEVICE_NET, test_rss_non_power_of_2,
              "RSS indirection_table_mask+1 not a power of 2",
              VIRTIO_SPEC_V1_2, "5.1.6.5.7.2");
