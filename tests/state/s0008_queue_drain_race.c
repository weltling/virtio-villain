/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0008: queue_drain_race
 *
 * Place multiple buffers in the available ring, kick, then immediately
 * reset the device and access the used ring entries. The spec says the
 * driver MUST ensure a virtqueue isn't live (by device reset) before
 * removing exposed buffers (3.3.1).
 *
 * Here we do the inverse: reset, then immediately read used ring entries
 * that may have been partially filled by the device during its shutdown
 * path. This stresses the VMM's cleanup of in-flight descriptors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define NUM_REQUESTS 4

static test_result_t test_queue_drain_race(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdrs[NUM_REQUESTS];
    uint8_t *datas[NUM_REQUESTS];
    uint8_t *statuses[NUM_REQUESTS];

    /* Set up multiple in-flight read requests */
    for (int i = 0; i < NUM_REQUESTS; i++) {
        hdrs[i] = vv_alloc_pages(1);
        datas[i] = vv_alloc_pages(1);
        statuses[i] = vv_alloc_pages(1);

        hdrs[i]->type = VIRTIO_BLK_T_IN;
        hdrs[i]->ioprio = 0;
        hdrs[i]->sector = i;
        *statuses[i] = 0xFF;

        int base = i * 3;
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdrs[i]),
                           sizeof(*hdrs[i]), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(datas[i]),
                           512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
                           base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(statuses[i]),
                           1, VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, base);
    }

    /* Make all available at once */
    vring_raw_set_avail_idx(vr, NUM_REQUESTS);
    __sync_synchronize();

    /* Kick to start processing */
    virtio_pci_kick(dev, 0);

    /* Immediately reset without waiting for completion */
    dev->common->device_status = 0;
    __sync_synchronize();

    /*
     * Now access the used ring while the device may still be draining.
     * This is the race condition - reading used ring during async cleanup.
     */
    __sync_synchronize();
    uint16_t used_idx = vr->used->idx;
    for (uint16_t i = 0; i < used_idx && i < NUM_REQUESTS; i++) {
        volatile uint32_t id = vr->used->ring[i].id;
        volatile uint32_t len = vr->used->ring[i].len;
        (void)id;
        (void)len;
    }

    /* Also try to access descriptor memory that was "exposed" */
    for (int i = 0; i < NUM_REQUESTS; i++) {
        /* Touch the data buffers - if VMM wrote partial results, we see them */
        volatile uint8_t byte = datas[i][0];
        (void)byte;
    }

    usleep(100000);

    /*
     * Re-init and verify device survived the race.
     */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = 0;
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Verify with a clean request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0008, VIRTIO_PCI_DEVICE_BLK, test_queue_drain_race,
              "Submit multiple I/Os then immediately reset and read used ring",
              VIRTIO_SPEC_V1_2, "3.3.1");
