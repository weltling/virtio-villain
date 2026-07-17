/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0015: net_rss_no_feature
 *
 * Send VIRTIO_NET_CTRL_MQ_RSS_CONFIG without negotiating
 * VIRTIO_NET_F_RSS. Spec 5.1.6.5.7.2: driver MUST NOT send
 * RSS_CONFIG if the feature has not been negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rss_no_feature(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_rss_config *rss =
        (struct virtio_net_rss_config *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;

    memset(rss, 0, sizeof(*rss));
    rss->hash_types = 0x1;
    rss->indirection_table_mask = 0;
    rss->unclassified_queue = 0;
    rss->indirection_table[0] = 0;
    rss->max_tx_vq = 1;
    rss->hash_key_length = 40;
    memset(rss->hash_key_data, 0xAA, 40);

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

REGISTER_TEST_Q_REQUIRES(N0015, VIRTIO_PCI_DEVICE_NET, test_net_rss_no_feature,
              "RSS_CONFIG command without RSS feature",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
