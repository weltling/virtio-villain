/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0037: many reinit cycles
 *
 * Spec 4.1.4.3.2 requires reset to bring the device back to its
 * initial state, repeatable any number of times. This test loops
 * through reset and full reinit 50 times to surface resource
 * leaks, file descriptor exhaustion or stale state in the VMM
 * backend that only show up after many cycles. After the loop a
 * valid I/O confirms the device is still usable.
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
#define REINIT_CYCLES 50

static test_result_t test_many_reinit_cycles(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    struct vring nv;

    (void)vr;

    vring_alloc(&nv, 16);

    for (int i = 0; i < REINIT_CYCLES; i++) {
        virtio_pci_reset(dev);
        if (cfg->device_status != 0)
            TWEDGED("cfg->device_status != 0");

        cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
        __sync_synchronize();
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

        cfg->queue_select = 0;
        __sync_synchronize();
        cfg->queue_size = 16;
        cfg->queue_desc = nv.desc_phys;
        cfg->queue_avail = nv.avail_phys;
        cfg->queue_used = nv.used_phys;
        cfg->queue_msix_vector = 0xffff;
        cfg->queue_enable = 1;
        __sync_synchronize();
        cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
        __sync_synchronize();
    }
    nv.queue = 0;

    /* Final I/O to confirm device is alive */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    /* Fresh ring slots */
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

REGISTER_TEST(S0037, VIRTIO_PCI_DEVICE_BLK, test_many_reinit_cycles,
              "50 reset and reinit cycles then a valid I/O",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
