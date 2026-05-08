/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0149: kick to queue N notify offset must not complete queue M
 *
 * Spec 4.1.4.4 says queue_notify_off times notify_off_multiplier
 * locates the notification address per queue. A device that
 * collapses queues onto a single notify register might process
 * queue M when only queue N was kicked. Set up two queues, place
 * a request only on queue 1, write only to queue 1's derived
 * notify address, and verify queue 1 used ring advances while
 * queue 0 used ring stays at zero.
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

static test_result_t test_blk_notify_targets_one_q(struct virtio_dev *dev,
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

    /* Capture notify offsets while queue_select is still controlled */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t off0 = cfg->queue_notify_off;
    cfg->queue_select = 1;
    __sync_synchronize();
    uint16_t off1 = cfg->queue_notify_off;

    /* If both queues share the same notify slot the test cannot
     * distinguish them, so accept that as a known device choice. */
    if (off0 == off1)
        return TEST_SKIP;

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Only queue 1 has anything in the avail ring */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(&q1, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q1, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q1, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q1, 0, 0);
    vring_raw_set_avail_idx(&q1, 1);

    /* Write to queue 1 derived address only */
    volatile uint16_t *addr1 = (volatile uint16_t *)
        ((char *)dev->notify_base +
         (uint32_t)off1 * dev->notify_off_multiplier);
    *addr1 = 1;
    __sync_synchronize();

    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (q1.used->idx != 0)
            break;
        usleep(1000);
        waited++;
    }
    if (q1.used->idx == 0)
        TWEDGED("q1.used->idx == 0");
    if (*st != 0)
        TFAIL("*st != 0");

    /* Queue 0 must not have been touched */
    __sync_synchronize();
    if (q0.used->idx != 0)
        TFAIL("q0.used->idx != 0");

    return TEST_PASS;
}

REGISTER_TEST(B0149, VIRTIO_PCI_DEVICE_BLK, test_blk_notify_targets_one_q,
              "kick on queue 1 notify offset leaves queue 0 idle",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
