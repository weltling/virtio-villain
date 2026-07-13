/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0072: suspend_no_buffers_consumed
 *
 * After setting SUSPEND, make buffers available to a queue.
 * The device MUST NOT consume them while suspended.
 * Spec 3.2: device MUST NOT access any virtqueues while suspended.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_suspend_no_consume(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Suspend the device */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(200000);

    uint8_t st = cfg->device_status;
    if (!(st & VIRTIO_STATUS_SUSPEND))
        return TEST_SKIP;

    /* Now make a buffer available */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *stat = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *stat = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(stat), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, 0);

    /* Wait and check used ring is still empty */
    usleep(300000);
    if (vr->used->idx != 0)
        TFAIL("vr->used->idx != 0");

    /* Status byte must remain untouched */
    if (*stat != 0xFF)
        TFAIL("*stat != 0xFF");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(S0072, VIRTIO_PCI_DEVICE_BLK, test_suspend_no_consume,
              "Device must not consume buffers while suspended",
              VIRTIO_SPEC_V1_3, "3.2",
              (1ULL << VIRTIO_F_SUSPEND), 0);
