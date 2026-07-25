/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0028: blk_zone_open_resource_status
 *
 * Spec 5.2.6.6: a zone management open that would exceed the device
 * open zone resource limit must complete with the specific status
 * VIRTIO_BLK_S_ZONE_OPEN_RESOURCE, not a generic error. z0014 opens
 * more zones than the limit but never inspects the returned status.
 * Open a batch of distinct zones, wait for every request to complete,
 * and verify each status is either OK (accepted) or the reserved
 * ZONE_OPEN_RESOURCE code (rejected for resource reasons). A different
 * non zero code means the device signalled the resource limit with the
 * wrong status. Skips when the device is not zoned.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#define ZONE_SIZE_SECTORS 524288  /* 256 MiB zone at 512 bytes/sector */
#define NUM_OPENS 64

static test_result_t test_blk_zone_open_resource_status(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_ZONED)))
        return TEST_SKIP;

    uint8_t *status[NUM_OPENS];

    for (int i = 0; i < NUM_OPENS; i++) {
        struct virtio_blk_req *hdr = vv_alloc_pages(1);
        status[i] = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_ZONE_OPEN;
        hdr->reserved = 0;
        hdr->sector = (uint64_t)i * ZONE_SIZE_SECTORS;
        *status[i] = 0xFF;

        uint16_t base = (uint16_t)(i * 2);
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdr),
                           sizeof(*hdr), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(status[i]), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, base);
    }
    vring_raw_set_avail_idx(vr, NUM_OPENS);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, NUM_OPENS,
                                         VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    for (int i = 0; i < NUM_OPENS; i++) {
        uint8_t s = *status[i];
        if (s != VIRTIO_BLK_S_OK && s != VIRTIO_BLK_S_ZONE_OPEN_RESOURCE)
            TFAIL("open %d status %u, expected OK or ZONE_OPEN_RESOURCE",
                  i, s);
    }

    return TEST_PASS;
}

REGISTER_TEST(Z0028, VIRTIO_PCI_DEVICE_BLK,
              test_blk_zone_open_resource_status,
              "zone open beyond the limit returns ZONE_OPEN_RESOURCE",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
