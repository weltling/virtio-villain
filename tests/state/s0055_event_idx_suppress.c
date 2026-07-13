/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0055: EVENT_IDX suppresses notification when below used_event
 *
 * Spec 2.7.10 says when VIRTIO_F_RING_EVENT_IDX is negotiated the
 * device must not raise an interrupt unless the driver's
 * used_event is exceeded by the index of the completed entry.
 * Renegotiate with EVENT_IDX, set used_event far in the future,
 * submit a request, and verify the queue bit in ISR is not set
 * after the device completes it. Then set used_event to zero,
 * submit another request, and verify the queue bit fires. If the
 * device does not support the feature, skip.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static int submit_one(struct virtio_dev *dev, struct vring *vr,
                      uint16_t base, uint16_t avail_slot)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, base, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, base + 1);
    vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
    vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
    vring_raw_set_avail_idx(vr, avail_slot + 1);
    virtio_pci_kick(dev, vr->queue);

    for (int i = 0; i < VV_TIMEOUT_MS; i++) {
        __sync_synchronize();
        if (vr->used->idx > avail_slot)
            return 0;
        usleep(1000);
    }
    return -1;
}

static test_result_t test_event_idx_suppress(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    if (!(f0 & (1u << VIRTIO_F_EVENT_IDX)))
        return TEST_SKIP;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = (1u << VIRTIO_F_EVENT_IDX);
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    struct vring nv;
    vring_alloc(&nv, 16);
    vring_attach(dev, &nv, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    if (!dev->isr)
        return TEST_SKIP;

    /* Phase A: used_event far in future, ISR queue bit must stay clear */
    nv.avail->ring[nv.size] = 0xFFFE;
    __sync_synchronize();
    /* drain any prior pending */
    (void)*dev->isr;
    if (submit_one(dev, &nv, 0, 0) < 0)
        TWEDGED("submit_one(dev, &nv, 0, 0) < 0");
    __sync_synchronize();
    uint8_t isr_a = *dev->isr;
    if (isr_a & VIRTIO_PCI_ISR_QUEUE)
        TREJECT("isr_a & VIRTIO_PCI_ISR_QUEUE");

    /* Phase B: used_event = 0, next completion must raise ISR */
    nv.avail->ring[nv.size] = 1;
    __sync_synchronize();
    if (submit_one(dev, &nv, 3, 1) < 0)
        TWEDGED("submit_one(dev, &nv, 3, 1) < 0");
    __sync_synchronize();
    uint8_t isr_b = *dev->isr;
    if (!(isr_b & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(isr_b & VIRTIO_PCI_ISR_QUEUE)");

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(S0055, VIRTIO_PCI_DEVICE_BLK, test_event_idx_suppress,
              "EVENT_IDX suppresses ISR below used_event then fires",
              VIRTIO_SPEC_V1_2, "2.7.10",
              TEST_FLAG_NEEDS_ISR);
