/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0009: zone_report_offset_past_last_zone
 *
 * Request a zone report with a starting sector beyond the last zone.
 * The device should return an empty report or an error status.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

struct virtio_blk_zone_report_hdr {
    uint64_t nr_zones;
} __attribute__((packed));

#define VIRTIO_BLK_T_ZONE_REPORT 10
#define VIRTIO_BLK_F_ZONED       12

static test_result_t test_zone_report_past_end(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_ZONED)))
        return TEST_SKIP;

    /* Read capacity */
    volatile uint64_t *cap = (volatile uint64_t *)dev->device_cfg;
    uint64_t capacity = *cap;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_zone_report_hdr *report = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Request zone report starting past the end of the device */
    hdr->type = VIRTIO_BLK_T_ZONE_REPORT;
    hdr->ioprio = 0;
    hdr->sector = capacity + 1000; /* well past end */
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(report), sizeof(*report) + 64,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0009, VIRTIO_PCI_DEVICE_BLK, test_zone_report_past_end,
              "Zone report with sector offset past last zone",
              VIRTIO_SPEC_V1_2, "5.2.6.5");
