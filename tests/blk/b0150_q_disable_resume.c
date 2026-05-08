/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0150: disable queue 1 mid flight then re enable resumes work
 *
 * Spec 4.1.4.3.2 says queue_enable lets the driver gate a queue.
 * Submit on queue 0, then while traffic could still arrive on
 * queue 1 disable queue 1 with queue_enable=0, submit on queue
 * 1's avail ring (which the device must ignore), kick queue 1
 * once and confirm no completion arrives, then re enable queue
 * 1, kick again, and verify the previously parked request now
 * completes. Queue 0 must continue to function throughout.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

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

static test_result_t test_blk_q_disable_resume(struct virtio_dev *dev,
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

    struct vring q0, q1;
    vring_alloc(&q0, 16);
    vring_alloc(&q1, 16);
    vring_attach(dev, &q0, 0);
    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Sanity request on queue 0 */
    uint8_t *st0 = NULL;
    submit_read(&q0, 0, &st0);
    virtio_pci_kick(dev, 0);
    int waited = 0;
    while (waited < VV_TIMEOUT_MS && q0.used->idx == 0) {
        usleep(1000);
        waited++;
    }
    if (q0.used->idx == 0 || *st0 != 0)
        TWEDGED("q0.used->idx == 0 || *st0 != 0");

    /* Disable queue 1 */
    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();

    uint8_t *st1 = NULL;
    submit_read(&q1, 7, &st1);
    virtio_pci_kick(dev, 1);
    usleep(100000);
    __sync_synchronize();
    if (q1.used->idx != 0)
        TREJECT("q1.used->idx != 0");

    /* Re enable and kick again */
    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    waited = 0;
    while (waited < VV_TIMEOUT_MS && q1.used->idx == 0) {
        usleep(1000);
        waited++;
    }
    if (q1.used->idx == 0)
        TREJECT("q1.used->idx == 0");
    if (*st1 != 0)
        TFAIL("*st1 != 0");

    return TEST_PASS;
}

REGISTER_TEST(B0150, VIRTIO_PCI_DEVICE_BLK, test_blk_q_disable_resume,
              "disable queue then re enable resumes parked request",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
