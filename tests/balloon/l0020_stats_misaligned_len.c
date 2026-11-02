/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0020: balloon_stats_misaligned_len
 *
 * Submit a stats vq descriptor whose length is not a multiple of
 * sizeof(virtio_balloon_stat). Spec 5.5.6.1 says the device must
 * not partially parse the buffer. The device must reject or stay
 * silent without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_stats_misaligned(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_BALLOON_F_STATS_VQ))
        return TEST_SKIP;
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    struct vring stats_vr;
    vring_alloc(&stats_vr, 16);
    vring_attach(dev, &stats_vr, 2);

    uint8_t *buf = vv_alloc_pages(1);

    /* sizeof(virtio_balloon_stat) is 10 bytes; 17 is misaligned */
    vv_desc(&stats_vr, 0, buf, 17, 0, 0);
    vring_raw_set_avail(&stats_vr, 0, 0);
    vring_raw_set_avail_idx(&stats_vr, 1);

    return vv_kick_and_wait(dev, &stats_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0020, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_stats_misaligned,
              "Stats vq descriptor with misaligned length",
              VIRTIO_SPEC_V1_2, "5.5.6.1",
              (1ULL << VIRTIO_BALLOON_F_STATS_VQ), 3);
