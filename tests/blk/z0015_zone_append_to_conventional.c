/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0015: zone_append_to_conventional
 *
 * Submit a ZONE_APPEND targeting sector 0, which on a non zoned
 * default device is not part of any sequential zone. Spec v1.3
 * 5.2.6 says ZONE_APPEND must return UNSUPP or IOERR when
 * targeting a conventional or absent zone. The device must
 * remain alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_zone_append_conv(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0x33, 512);
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0015, VIRTIO_PCI_DEVICE_BLK, test_zone_append_conv,
              "ZONE_APPEND to a conventional or absent zone",
              VIRTIO_SPEC_V1_3, "5.2.6");
