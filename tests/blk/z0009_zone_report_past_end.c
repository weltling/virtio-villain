/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0009: zone_report_offset_past_last_zone
 *
 * Request a zone report with a starting sector beyond the last zone.
 * The device should return an empty report or an error status.
 */
#include "lib/virtio_test.h"

static test_result_t test_zone_report_past_end(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_ZONED))
        return TEST_SKIP;

    /* Read capacity */
    volatile uint64_t *cap = (volatile uint64_t *)dev->device_cfg;
    uint64_t capacity = *cap;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_ZONE_REPORT, 0);
    struct virtio_blk_zone_report_hdr_wide *report = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Request zone report starting past the end of the device */
    hdr->sector = capacity + 1000; /* well past end */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, report, sizeof(*report) + 64,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(Z0009, VIRTIO_PCI_DEVICE_BLK, test_zone_report_past_end,
              "Zone report with sector offset past last zone",
              VIRTIO_SPEC_V1_2, "5.2.6.5",
              (1ULL << VIRTIO_BLK_F_ZONED), 0);
