/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0152: queues with different ring sizes both serve I/O
 *
 * Spec 4.1.4.3.2 says queue_size is set per queue. The driver may
 * pick any power of two up to the device advertised maximum, and
 * different queues may use different sizes. Configure queue 0
 * with size 64 and queue 1 with size 16, submit a read on each,
 * and verify both complete with status OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static int submit_read(struct vring *vr, uint64_t sector, uint8_t **st_out)
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

static test_result_t test_blk_mixed_qsizes(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    (void)vr;

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

    /* Probe maximum size for queue 0 */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t qmax = cfg->queue_size;
    uint16_t big = qmax >= 64 ? 64 : qmax;
    if (big < 8)
        return TEST_SKIP;

    struct vring q0, q1;
    vring_alloc(&q0, big);
    vring_alloc(&q1, 16);
    vring_attach(dev, &q0, 0);
    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    uint8_t *st0 = NULL, *st1 = NULL;
    submit_read(&q0, 0, &st0);
    submit_read(&q1, 1, &st1);
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

REGISTER_TEST_REQUIRES(B0152, VIRTIO_PCI_DEVICE_BLK, test_blk_mixed_qsizes,
              "queues with different ring sizes both serve reads",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2",
              0, 2);
