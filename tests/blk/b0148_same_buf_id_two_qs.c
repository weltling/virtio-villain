/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0148: same buffer head id submitted on two queues at once
 *
 * Spec 2.7.5 says descriptor table indexes are private to each
 * virtqueue. Submitting the same buffer head id on two queues
 * simultaneously must not confuse the device, because each queue
 * has its own descriptor memory. Set up two queues with separate
 * descriptor tables, place a complete read chain at index 0 on
 * each, kick both, and verify both used rings advance and both
 * status bytes report OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static int build_read(struct vring *vr, uint64_t sector, uint8_t **st_out)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = sector;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    *st_out = st;
    return 0;
}

static test_result_t test_blk_same_buf_id_two_qs(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    (void)vr;

    /* Re init so we can stand up two queues before DRIVER_OK */
    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring q0, q1;
    vring_alloc(&q0, 16);
    vring_alloc(&q1, 16);
    vring_attach(dev, &q0, 0);
    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    uint8_t *st0 = NULL, *st1 = NULL;
    build_read(&q0, 0, &st0);
    build_read(&q1, 1, &st1);
    __sync_synchronize();

    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (q0.used->idx != 0 && q1.used->idx != 0)
            break;
        usleep(1000);
        waited++;
    }
    if (q0.used->idx == 0 || q1.used->idx == 0)
        TWEDGED("q0.used->idx == 0 || q1.used->idx == 0");

    if (*st0 != 0 || *st1 != 0)
        TFAIL("*st0 != 0 || *st1 != 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0148, VIRTIO_PCI_DEVICE_BLK, test_blk_same_buf_id_two_qs,
              "same head id on two queues completes independently",
              VIRTIO_SPEC_V1_2, "2.7.5",
              0, 2);
