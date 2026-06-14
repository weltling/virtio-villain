/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0019: packed_event_wrap_mismatch
 *
 * Set the device event suppression structure (device_event) with
 * event index mode (flags=2) but encode a wrap counter that doesn't
 * match the current device position. This tests whether the device
 * correctly compares wrap counters when deciding to suppress events.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_event_wrap_mismatch(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
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

    /*
     * Set device_event to DESC mode with wrong wrap counter.
     * off_wrap field: (desc_event_off | (wrap << 15))
     * Use wrap=0 when device expects wrap=1.
     */
    volatile uint32_t *device_event = (volatile uint32_t *)vr->device_event;
    uint16_t wrong_wrap = 0; /* device starts with wrap=1 */
    uint16_t off_wrap = (0) | (wrong_wrap << 15);
    *device_event = VRING_PACKED_EVENT_FLAG_DESC | ((uint32_t)off_wrap << 16);
    __sync_synchronize();

    /* Submit a valid request */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_set_desc(vr, 2, status_phys, 1, 2,
                          VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0019, VIRTIO_PCI_DEVICE_BLK, test_packed_event_wrap_mismatch,
                     "Device event suppression with mismatched wrap counter",
                     VIRTIO_SPEC_V1_2, "2.8.10");
