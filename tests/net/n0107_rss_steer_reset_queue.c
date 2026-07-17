/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0107: net_rss_steer_to_reset_queue
 *
 * Configure RSS to steer packets to a queue that has been reset.
 * Spec v1.3 5.1.6.5.4 + 2.6.1: the device must handle steering
 * to a reset queue gracefully (drop or redirect).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rss_reset_queue(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_RSS))
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset queue 0 (RX) first via queue_enable = 0 */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();

    /* Now configure RSS to steer to queue 0 via ctrl VQ */
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_rss_config_full *rss =
        (struct virtio_net_rss_config_full *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;
    rss->hash_types = 0x01;
    rss->indirection_table_mask = 0x0F;
    rss->unclassified_queue = 0;  /* Points to reset queue */
    for (int i = 0; i < 16; i++)
        rss->indirection_table[i] = 0;  /* All point to reset queue */
    rss->max_tx_vq = 1;
    rss->hash_key_length = 40;
    memset(rss->hash_key, 0x6D, 40);
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*rss),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0107, VIRTIO_PCI_DEVICE_NET, test_net_rss_reset_queue,
                "RSS steer to queue that has been reset",
                VIRTIO_SPEC_V1_3, "5.1.6.5.4", VV_QUEUE_LAST,
                (1ULL << VIRTIO_NET_F_CTRL_VQ) |
                (1ULL << VIRTIO_NET_F_RSS), 0);
