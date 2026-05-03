/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0021: rapid_reset_cycle
 *
 * Perform DRIVER_OK -> reset -> DRIVER_OK in a very tight loop.
 * Tests device resilience to rapid lifecycle recycling where
 * internal state teardown and setup may overlap.
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

static test_result_t test_rapid_reset_cycle(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Do 10 rapid reset cycles */
    for (int i = 0; i < 10; i++) {
        /* Reset */
        cfg->device_status = 0;
        __sync_synchronize();
        /* Don't wait for completion - immediately proceed */

        /* ACKNOWLEDGE + DRIVER */
        cfg->device_status = 1 | 2;
        __sync_synchronize();

        /* Read and accept offered features (word 0 only) */
        cfg->device_feature_select = 0;
        __sync_synchronize();
        uint32_t feat = cfg->device_feature;
        cfg->driver_feature_select = 0;
        cfg->driver_feature = feat;
        __sync_synchronize();

        /* FEATURES_OK */
        cfg->device_status = 1 | 2 | 8;
        __sync_synchronize();

        /* DRIVER_OK */
        cfg->device_status = 1 | 2 | 8 | 4;
        __sync_synchronize();
    }

    /* Final cycle: actually do I/O to verify device is functional */
    virtio_pci_reset(dev);

    cfg->device_status = 1 | 2;
    __sync_synchronize();
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = feat;
    __sync_synchronize();
    cfg->device_status = 1 | 2 | 8;
    __sync_synchronize();
    usleep(1000);
    if (!(cfg->device_status & 8))
        TREJECT("!(cfg->device_status & 8)");
    cfg->device_status = 1 | 2 | 8 | 4;
    __sync_synchronize();

    struct vring tvr;
    vring_alloc(&tvr, 16);
    vring_attach(dev, &tvr, 0);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&tvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&tvr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&tvr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    return vv_kick_and_wait(dev, &tvr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0021, VIRTIO_PCI_DEVICE_BLK, test_rapid_reset_cycle,
              "Rapid DRIVER_OK -> reset -> DRIVER_OK cycling (10x)",
              VIRTIO_SPEC_V1_2, "2.2");
