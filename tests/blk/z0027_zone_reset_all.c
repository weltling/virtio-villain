/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0027: Zone RESET_ALL with data pending.
 *
 * Spec v1.3 5.2.6: Submit ZONE_RESET_ALL (type 48). This should
 * reset all zones unconditionally. Verify the device handles it
 * without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_zone_reset_all(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_RESET_ALL;
    hdr->reserved = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0027, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_reset_all,
              "Zone RESET_ALL command",
              VIRTIO_SPEC_V1_3, "5.2.6");
