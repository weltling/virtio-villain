/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0062: VIRTIO_BLK_T_GET_LIFETIME command (spec 5.2.6.3)
 *
 * Issue a GET_LIFETIME request. If the feature is not supported,
 * the device should reject or return an error status.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_get_lifetime(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_lifetime *life = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_LIFETIME;
    hdr->ioprio = 0;
    hdr->sector = 0;

    memset(life, 0, sizeof(*life));
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(life), sizeof(*life),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0062, VIRTIO_PCI_DEVICE_BLK, test_get_lifetime,
              "VIRTIO_BLK_T_GET_LIFETIME command",
              VIRTIO_SPEC_V1_2, "5.2.6.3");
