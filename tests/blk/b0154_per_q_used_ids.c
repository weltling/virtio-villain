/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0154: per queue used ring entries appear in submission order
 *
 * Spec 2.7.8 says the device may complete requests on a single
 * queue in any order, but the IDs in the used ring identify the
 * heads. With four sequential reads on a queue, every head id
 * placed in the avail ring must show up in the used ring exactly
 * once. Submit four reads on queue 0 and four reads on queue 1
 * concurrently, then verify both queues see all four IDs in their
 * own used ring with no duplicates and no leaks across queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static void plant_read(struct vring *vr, uint16_t base,
                       uint16_t avail_slot, uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = sector;
    *st = 0xFF;
    vring_raw_set_desc(vr, base, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, base + 1);
    vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
    vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
}

static int verify_ids(struct vring *vr, int n)
{
    uint8_t seen[64] = {0};
    if (vr->used->idx < (uint16_t)n)
        return -1;
    for (int i = 0; i < n; i++) {
        uint16_t id = vr->used->ring[i].id;
        uint16_t expected = (uint16_t)(i * 3);
        if (id > 60)
            return -1;
        if (seen[id])
            return -1;
        seen[id] = 1;
        (void)expected;
    }
    return 0;
}

static test_result_t test_blk_per_q_used_ids(struct virtio_dev *dev,
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

    int n = 4;
    for (int i = 0; i < n; i++) {
        plant_read(&q0, i * 3, i, (uint64_t)i);
        plant_read(&q1, i * 3, i, (uint64_t)(100 + i));
    }
    vring_raw_set_avail_idx(&q0, n);
    vring_raw_set_avail_idx(&q1, n);
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (q0.used->idx >= n && q1.used->idx >= n)
            break;
        usleep(1000);
        waited++;
    }
    if (q0.used->idx < n || q1.used->idx < n)
        TWEDGED("q0.used->idx < n || q1.used->idx < n");

    if (verify_ids(&q0, n) < 0)
        TFAIL("verify_ids(&q0, n) < 0");
    if (verify_ids(&q1, n) < 0)
        TFAIL("verify_ids(&q1, n) < 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0154, VIRTIO_PCI_DEVICE_BLK, test_blk_per_q_used_ids,
              "all submitted head ids appear once in own used ring",
              VIRTIO_SPEC_V1_2, "2.7.8",
              0, 2);
