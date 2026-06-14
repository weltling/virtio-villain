/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0041: packed_event_off_at_qsize
 *
 * Set driver_event.off_wrap to qsize, an index one past the
 * largest valid descriptor slot. Spec 2.8.10 leaves the value
 * undefined past qsize. The device must not enable a phantom
 * slot or panic; it should treat the suppression hint as
 * disabled.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_event_off_qsize(struct virtio_dev *dev,
                                                 struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vr->driver_event->flags = 2; /* desc */
    vr->driver_event->off_wrap = vr->size;
    __sync_synchronize();

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, 1,
                          VRING_PACKED_DESC_F_NEXT
                          | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0041, VIRTIO_PCI_DEVICE_BLK, test_packed_event_off_qsize,
                     "driver_event off_wrap equal to qsize",
                     VIRTIO_SPEC_V1_2, "2.8.10");
