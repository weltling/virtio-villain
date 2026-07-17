/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0127: RSS context configuration without RSS feature.
 *
 * v1.4 5.1.4 plus VIRTIO_NET_F_RSS (bit 60). If RSS is not
 * negotiated the driver MUST NOT send RSS ctrl messages. Send
 * a VIRTIO_NET_CTRL_MQ_RSS_CONFIG (class 4 cmd 1) and verify
 * the device rejects.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    if (cfg->device_feature & (1U << (VIRTIO_NET_F_RSS - 32)))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *h = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);
    h->class = VIRTIO_NET_CTRL_MQ;
    h->command = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;
    memset(payload, 0, 256);
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h), sizeof(*h),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 256,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0127, VIRTIO_PCI_DEVICE_NET, test,
                "RSS config ctrl message without RSS feature",
                VIRTIO_SPEC_V1_4, "5.1.4", VV_QUEUE_LAST,
                (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
