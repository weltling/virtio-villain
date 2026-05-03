/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0031: packed driver event flags set to reserved value 3
 *
 * Spec 2.8.10: "flags takes values 0, 1, or 2" (ENABLE, DISABLE,
 * DESC). Value 3 is reserved and MUST NOT be used by the driver.
 *
 * Set driver_event.flags to 3 and submit a request to see how the
 * device handles the reserved notification suppression value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0
#define RING_EVENT_FLAGS_RESERVED 3

static test_result_t test_packed_event_reserved(struct virtio_dev *dev,
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

    /* Set driver event flags to reserved value 3 */
    vr->driver_event->flags = RING_EVENT_FLAGS_RESERVED;
    vr->driver_event->off_wrap = 0;
    __sync_synchronize();

    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, data_phys, 512, 0,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_set_desc(vr, 2, status_phys, 1, 0,
                          VRING_PACKED_DESC_F_WRITE);

    uint8_t check_wrap = vr->wrap_counter;
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (vring_packed_desc_is_used(vr, 0, check_wrap))
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0031, VIRTIO_PCI_DEVICE_BLK, test_packed_event_reserved,
                     "Packed driver event flags set to reserved value 3",
                     VIRTIO_SPEC_V1_2, "2.8.10");
