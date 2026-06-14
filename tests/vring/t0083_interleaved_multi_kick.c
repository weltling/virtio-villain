/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0083: Multiple queues with interleaved kicks (spec 2.7.13)
 *
 * Set up all available queues and kick them in rapid interleaved
 * fashion without waiting for completions. Tests device handling
 * of rapid multi-queue stimulus.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_interleaved_multi_kick(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        return TEST_SKIP;
    if (nq > 8)
        nq = 8; /* cap at 8 for resource reasons */

    /* Set up extra queues (queue 0 is vr) */
    struct vring extra[7];
    for (uint16_t q = 1; q < nq; q++) {
        vring_alloc(&extra[q - 1], 16);
        vring_attach(dev, &extra[q - 1], q);
    }

    /* Submit a request on each queue */
    for (uint16_t q = 0; q < nq; q++) {
        struct vring *qvr = (q == 0) ? vr : &extra[q - 1];
        struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *st = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_IN;
        hdr->ioprio = 0;
        hdr->sector = q;
        *st = 0xFF;

        vring_raw_set_desc(qvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(qvr, 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_desc(qvr, 2, vv_virt_to_phys(st), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(qvr, 0, 0);
        vring_raw_set_avail_idx(qvr, 1);
    }

    /* Kick all queues in rapid succession */
    __sync_synchronize();
    for (uint16_t q = 0; q < nq; q++)
        virtio_pci_kick(dev, q);

    /* Wait for at least one completion */
    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    int any = 0;
    if (vr->used->idx != 0)
        any = 1;
    for (uint16_t q = 1; q < nq && !any; q++) {
        if (extra[q - 1].used->idx != 0)
            any = 1;
    }

    if (any)
        return TEST_PASS;

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0083, VIRTIO_PCI_DEVICE_BLK, test_interleaved_multi_kick,
              "Interleaved kicks across all available queues",
              VIRTIO_SPEC_V1_2, "2.7.13");
