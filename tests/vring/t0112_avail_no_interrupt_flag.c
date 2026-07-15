/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0112: AVAIL_F_NO_INTERRUPT suppresses used buffer notifications.
 *
 * Spec 2.7.7: When the driver sets VRING_AVAIL_F_NO_INTERRUPT in
 * the avail flags, the device SHOULD NOT send interrupts. Set the
 * flag, submit a request, verify it completes (used ring advances)
 * and the device remains healthy. The test cannot directly verify
 * interrupt suppression (no IRQ handler) but exercises the flag path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VRING_AVAIL_F_NO_INTERRUPT 1

static test_result_t test_avail_no_interrupt(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /* Set NO_INTERRUPT flag in avail ring */
    vr->avail->flags = VRING_AVAIL_F_NO_INTERRUPT;
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
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("status %u with NO_INTERRUPT flag set", *st);

    /* Clear the flag for clean teardown */
    vr->avail->flags = 0;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(T0112, VIRTIO_PCI_DEVICE_BLK, test_avail_no_interrupt,
              "Request completes with AVAIL_F_NO_INTERRUPT set",
              VIRTIO_SPEC_V1_2, "2.7.7");
