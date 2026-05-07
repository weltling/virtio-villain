/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0094: USED_F_NO_NOTIFY in used flags is a hint
 *
 * Spec 2.7.7 says VRING_USED_F_NO_NOTIFY in used->flags is set by
 * the device to suppress driver notifications. The driver may also
 * write the field, but its value is the device side hint and the
 * device is free to ignore it. Writing the bit from the driver
 * must not break processing of subsequent requests.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VRING_USED_F_NO_NOTIFY 1

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_used_no_notify_hint(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Driver scribbles the device hint flag */
    vr->used->flags = VRING_USED_F_NO_NOTIFY;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    /* Reset the hint */
    vr->used->flags = 0;
    __sync_synchronize();

    return r;
}

REGISTER_TEST(T0094, VIRTIO_PCI_DEVICE_BLK, test_used_no_notify_hint,
              "device serves request after driver writes USED_F_NO_NOTIFY",
              VIRTIO_SPEC_V1_2, "2.7.7");
