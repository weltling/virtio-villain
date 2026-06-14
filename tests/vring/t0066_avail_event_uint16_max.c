/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0066: avail_event_uint16_max
 *
 * Set the avail_event (used_event) field to UINT16_MAX. With event
 * suppression enabled, this extreme value tests whether the device
 * correctly handles the wrap-around comparison for notification
 * suppression.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_avail_event_max(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /*
     * Set used_event (at end of avail ring) to UINT16_MAX.
     * The used_event is at avail->ring[queue_size].
     * Use memcpy to avoid unaligned pointer warning.
     */
    uint16_t event_val = 0xFFFF;
    memcpy((void *)&vr->avail->ring[vr->size], &event_val, sizeof(event_val));
    __sync_synchronize();

    /* Enable event idx (clear NO_INTERRUPT flag) */
    vr->avail->flags = 0;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0066, VIRTIO_PCI_DEVICE_BLK, test_avail_event_max,
              "used_event set to UINT16_MAX (event suppression edge)",
              VIRTIO_SPEC_V1_2, "2.7.8");
