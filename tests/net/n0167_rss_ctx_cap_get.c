/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0167: net_rss_ctx_cap_get
 *
 * With VIRTIO_NET_F_RSS_CONTEXT (bit 64) negotiated, query the RSS
 * context capability via the VIRTNET_RSS_CTX_CTRL_CAP_GET command and
 * verify the device reports at least one context. Spec 5.1.6.5.7 RSS
 * Context: the device MUST set max_rss_contexts to at least 1 if it
 * offers VIRTIO_NET_F_RSS_CONTEXT. The feature requires CTRL_VQ and
 * RSS. n0164 covers the negative path where the feature is absent;
 * this is the positive capability query. Skips when the device does
 * not offer RSS_CONTEXT, as CH does not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_rss_ctx_cap_get(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_RSS) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_RSS_CONTEXT))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *h = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    h->class = VIRTIO_NET_CTRL_RSS_CTX;
    h->command = VIRTIO_NET_CTRL_RSS_CTX_CAP_GET;
    memset(resp, 0, 256);
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h), sizeof(*h),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), 2,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("CAP_GET acked 0x%02x, expected VIRTIO_NET_OK", *ack);

    uint16_t max_rss_contexts = (uint16_t)(resp[0] | (resp[1] << 8));
    if (max_rss_contexts < 1)
        TFAIL("max_rss_contexts is 0 with RSS_CONTEXT offered");

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0167, VIRTIO_PCI_DEVICE_NET,
                         test_net_rss_ctx_cap_get,
                         "RSS context CAP_GET reports at least one context",
                         VIRTIO_SPEC_V1_4, "5.1.6.5.7", VV_QUEUE_LAST,
                         VV_FEATURE_BIT(VIRTIO_NET_F_CTRL_VQ) |
                         VV_FEATURE_BIT(VIRTIO_NET_F_RSS) |
                         VV_FEATURE_BIT(VIRTIO_NET_F_RSS_CONTEXT), 0);
