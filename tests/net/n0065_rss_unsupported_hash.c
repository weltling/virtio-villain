/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0065: RSS config with unsupported hash type bits
 *
 * Spec 5.1.6.5.7.2: "A driver MUST NOT set any VIRTIO_NET_HASH_TYPE_
 * flags that are not supported by a device."
 *
 * Send RSS_CONFIG with all hash_types bits set (0xFFFFFFFF),
 * including those not in supported_hash_types.
 */
#include "lib/virtio_test.h"

static test_result_t test_rss_unsupported_hash(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_RSS))
        return TEST_SKIP;

    uint16_t nq = cfg->num_queues;
    if (nq < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *rss_buf = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;

    /* hash_types = 0xFFFFFFFF (all bits, most unsupported) */
    uint32_t hash_types = 0xFFFFFFFF;
    memcpy(rss_buf, &hash_types, 4);
    /* indirection_table_mask = 0 → 1 entry (power of 2) */
    uint16_t mask = 0;
    memcpy(rss_buf + 4, &mask, 2);
    /* unclassified_queue = 0 */
    uint16_t unclass = 0;
    memcpy(rss_buf + 6, &unclass, 2);
    /* indirection_table: 1 entry */
    uint16_t table = 0;
    memcpy(rss_buf + 8, &table, 2);
    /* max_tx_vq = 0 */
    uint16_t max_tx = 0;
    memcpy(rss_buf + 10, &max_tx, 2);
    /* hash_key_length = 40 */
    rss_buf[12] = 40;
    memset(rss_buf + 13, 0xAA, 40);

    uint16_t rss_len = 13 + 40;
    *ack = 0xFF;

    vv_desc(&cvr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(&cvr, 1, rss_buf, rss_len, VRING_DESC_F_NEXT, 2);
    vv_desc(&cvr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(N0065, VIRTIO_PCI_DEVICE_NET, test_rss_unsupported_hash,
                  "RSS_CONFIG with all hash_types bits set (unsupported flags)",
                  VIRTIO_SPEC_V1_2, "5.1.6.5.7.2");
