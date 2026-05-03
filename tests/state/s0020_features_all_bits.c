/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0020: feature_negotiation_all_bits_set
 *
 * Attempt to negotiate by accepting ALL 128 feature bits offered and
 * not-offered. The driver sets every bit in driver_feature. The device
 * must either accept only the bits it offered or reject FEATURES_OK.
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

static test_result_t test_features_all_bits(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset device */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER */
    cfg->device_status = 1 | 2;
    __sync_synchronize();

    /* Set ALL feature bits (words 0-3) */
    for (uint32_t sel = 0; sel < 4; sel++) {
        cfg->driver_feature_select = sel;
        __sync_synchronize();
        cfg->driver_feature = 0xFFFFFFFF;
        __sync_synchronize();
    }

    /* FEATURES_OK */
    cfg->device_status = 1 | 2 | 8;
    __sync_synchronize();
    usleep(10000);

    uint8_t status = cfg->device_status;
    if (!(status & 8)) {
        /* Device rejected FEATURES_OK - that's a valid response */
        TREJECT("!(status & 8)");
    }

    /* Device accepted - proceed to DRIVER_OK and do a simple I/O */
    cfg->device_status = status | 4;
    __sync_synchronize();

    /* Set up a simple queue and do a read */
    struct vring tvr;
    vring_alloc(&tvr, 16);
    vring_attach(dev, &tvr, 0);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t st_phys = vv_virt_to_phys(st);

    vring_raw_set_desc(&tvr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&tvr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&tvr, 2, st_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    return vv_kick_and_wait(dev, &tvr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0020, VIRTIO_PCI_DEVICE_BLK, test_features_all_bits,
              "Feature negotiation accepting all 128 bits",
              VIRTIO_SPEC_V1_2, "2.2");
