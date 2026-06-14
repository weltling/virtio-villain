/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0038: use_feature_not_negotiated
 *
 * Send a DISCARD request without negotiating VIRTIO_BLK_F_DISCARD.
 * The device should return UNSUPP or reject, not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_use_feature_not_negotiated(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Discard request header */
    hdr->type = VIRTIO_BLK_T_DISCARD;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Discard segment: sector=0, num_sectors=1, flags=0 */
    memset(data, 0, 512);
    uint64_t *seg_sector = (uint64_t *)data;
    uint32_t *seg_num = (uint32_t *)(data + 8);
    *seg_sector = 0;
    *seg_num = 1;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0038, VIRTIO_PCI_DEVICE_BLK, test_use_feature_not_negotiated,
              "DISCARD request without negotiating feature",
              VIRTIO_SPEC_V1_2, "5.2.6");
