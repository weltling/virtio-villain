/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0165: net_device_stats_no_feature
 *
 * Send a VIRTIO_NET_CTRL_STATS command (control class 8, subcommand
 * STATS_QUERY) without negotiating VIRTIO_NET_F_DEVICE_STATS (bit
 * 50). Spec 5.1.6.5 Device Statistics: the statistics control
 * commands are available only when DEVICE_STATS is negotiated. A
 * device that has not negotiated the feature must handle the
 * unrecognised command gracefully, acking VIRTIO_NET_ERR rather than
 * crashing or wedging. n0125 covers the positive path that needs the
 * feature; this is the negative path. The control queue needs
 * CTRL_VQ, so require it and skip when it is not offered. Skip when
 * the device does offer DEVICE_STATS, since then the command is
 * valid.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_device_stats_no_feature(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;
    if (virtio_pci_feature_offered(dev, VIRTIO_NET_F_DEVICE_STATS))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *h = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    h->class = VIRTIO_NET_CTRL_STATS;
    h->command = VIRTIO_NET_CTRL_STATS_QUERY;
    memset(payload, 0, 256);
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h), sizeof(*h),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 256,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != VIRTIO_NET_ERR)
        TFAIL("device acked 0x%02x, expected VIRTIO_NET_ERR for a "
              "statistics command without DEVICE_STATS", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0165, VIRTIO_PCI_DEVICE_NET,
                         test_net_device_stats_no_feature,
                         "device statistics ctrl command without feature",
                         VIRTIO_SPEC_V1_4, "5.1.6.5", VV_QUEUE_LAST,
                         (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
