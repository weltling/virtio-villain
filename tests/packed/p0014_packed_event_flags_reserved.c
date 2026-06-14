/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0014: packed_event_flags_reserved
 *
 * Set driver_event.flags to a reserved value (3). Valid values are
 * 0 (enable), 1 (disable), 2 (desc). Value 3 is undefined and could
 * cause the device to misinterpret notification suppression state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_event_flags_reserved(struct virtio_dev *dev,
                                                      struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Set driver_event.flags to reserved value 3 */
    vr->driver_event->flags = 3;
    vr->driver_event->off_wrap = 0;
    __sync_synchronize();

    /* Submit a normal request */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0014, VIRTIO_PCI_DEVICE_BLK, test_packed_event_flags_reserved,
                     "driver_event.flags set to reserved value 3",
                     VIRTIO_SPEC_V1_2, "2.8.10");
