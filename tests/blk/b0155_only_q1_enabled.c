/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0155: queue 1 still serves I/O after queue 0 is left disabled
 *
 * Spec 4.1.4.3.2 says queue_enable is per queue. A driver that
 * brings up only queue 1, leaving queue 0 disabled, must still
 * see queue 1 work normally. Set up two queues, leave queue 0
 * with queue_enable zero, enable queue 1, set DRIVER_OK, submit
 * a read on queue 1 and verify the used ring advances and the
 * status byte reads OK.
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

static test_result_t test_blk_only_q1_enabled(struct virtio_dev *dev,
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

    /* Allocate q0 backing memory but leave it disabled */
    struct vring q0, q1;
    vring_alloc(&q0, 16);
    vring_alloc(&q1, 16);

    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = q0.size;
    cfg->queue_desc = q0.desc_phys;
    cfg->queue_avail = q0.avail_phys;
    cfg->queue_used = q0.used_phys;
    cfg->queue_msix_vector = 0xFFFF;
    cfg->queue_enable = 0;
    __sync_synchronize();

    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

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
    virtio_pci_kick(dev, 1);

    int waited = 0;
    while (waited < VV_TIMEOUT_MS && q1.used->idx == 0) {
        usleep(1000);
        waited++;
    }
    if (q1.used->idx == 0)
        TWEDGED("q1.used->idx == 0");
    if (*st != 0)
        TFAIL("*st != 0");

    /* Queue 0 must remain idle */
    __sync_synchronize();
    if (q0.used->idx != 0)
        TFAIL("q0.used->idx != 0");

    return TEST_PASS;
}

REGISTER_TEST(B0155, VIRTIO_PCI_DEVICE_BLK, test_blk_only_q1_enabled,
              "queue 1 serves I/O while queue 0 stays disabled",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
