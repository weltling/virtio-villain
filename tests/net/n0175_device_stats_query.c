/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0175: net_device_stats_query
 *
 * Spec 5.1.6.5.9: with VIRTIO_NET_F_DEVICE_STATS negotiated the driver
 * queries the supported statistics by sending VIRTIO_NET_CTRL_STATS
 * with subcommand STATS_QUERY on the control queue. The device fills a
 * device writable virtio_net_stats_capabilities with the bitmap of
 * supported stat types and acks VIRTIO_NET_OK. n0125 issues a bare
 * STATS_GET with only a one byte reply and no proper structures; this
 * drives the query with the real capabilities reply and checks the ack.
 * Skips when the device does not offer DEVICE_STATS or the control vq.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_net_device_stats_query(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_DEVICE_STATS) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    struct virtio_net_stats_capabilities *caps = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_STATS;
    hdr->command = VIRTIO_NET_CTRL_STATS_QUERY;
    caps->supported_stats_types = 0;
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(caps), sizeof(*caps),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("STATS_QUERY ack %u, expected VIRTIO_NET_OK", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0175, VIRTIO_PCI_DEVICE_NET,
              test_net_device_stats_query,
              "DEVICE_STATS query returns capabilities and acks OK",
              VIRTIO_SPEC_V1_4, "5.1.6.5.9", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_DEVICE_STATS) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
