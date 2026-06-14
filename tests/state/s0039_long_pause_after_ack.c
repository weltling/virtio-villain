/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0039: long pause between ACKNOWLEDGE and DRIVER
 *
 * Spec 3.1.1 lets the driver take any amount of time between status
 * bits. After reset, set ACKNOWLEDGE, sleep 200 ms, then continue
 * the rest of the init sequence and run a valid block read. A VMM
 * that times out partial init or assumes back to back status writes
 * will misbehave.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_long_pause_after_ack(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Long pause before continuing */
    usleep(200000);

    if (cfg->device_status != VIRTIO_STATUS_ACKNOWLEDGE)
        TFAIL("cfg->device_status != VIRTIO_STATUS_ACKNOWLEDGE");

    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    struct vring nv;
    vring_alloc(&nv, 16);
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = 16;
    cfg->queue_desc = nv.desc_phys;
    cfg->queue_avail = nv.avail_phys;
    cfg->queue_used = nv.used_phys;
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();
    nv.queue = 0;

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(&nv, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&nv, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&nv, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&nv, 0, 0);
    vring_raw_set_avail_idx(&nv, 1);

    return vv_kick_and_wait(dev, &nv, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0039, VIRTIO_PCI_DEVICE_BLK, test_long_pause_after_ack,
              "200ms pause between ACKNOWLEDGE and DRIVER then full init",
              VIRTIO_SPEC_V1_2, "3.1.1");
