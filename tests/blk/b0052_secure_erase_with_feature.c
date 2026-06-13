/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0052: secure_erase_with_feature
 *
 * Negotiate VIRTIO_BLK_F_SECURE_ERASE and submit a secure erase
 * command for sector 0. Verify the device completes successfully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_secure_erase(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if device offers SECURE_ERASE (feature bit 16, word 0) */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_SECURE_ERASE)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_SECURE_ERASE;
    hdr->ioprio = 0;
    hdr->sector = 0;

    seg->sector = 0;
    seg->num_sectors = 8; /* erase 8 sectors */
    seg->flags = 0;

    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0052, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase,
              "Secure erase with VIRTIO_BLK_F_SECURE_ERASE negotiated",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
