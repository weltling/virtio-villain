/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0128: block write readable data descriptor addr plus len wraps 2^64.
 *
 * T0008 and T0105 wrap a writable data descriptor on a read request,
 * which exercises the device write into guest memory. Here the request
 * is a write, so the data descriptor is device readable and the device
 * reads from the wrapping range. The read and write access paths can be
 * separate, so a wrap check on one does not imply the other. The base
 * sits near the top of the address space and the length makes addr plus
 * len wrap to a low value. The device must reject the range or stay
 * alive.
 *
 * Spec 2.7.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_write_readable_wrap(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type   = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status     = 0xFF;

    /*
     * Data descriptor is device readable and wraps: base 2^64 - 4096,
     * len 0x2000 -> end wraps to 0x1000.
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0128, VIRTIO_PCI_DEVICE_BLK, test_blk_write_readable_wrap,
              "Block write readable data addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "2.7.5");
