/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0034: reinit with reduced feature subset
 *
 * Spec 3.1.1 step 5 says the driver writes the subset of offered
 * features it accepts. After a reset, the driver may legitimately
 * choose a strictly smaller subset. This test resets the device,
 * reads the offered features, and renegotiates with only a single
 * mandatory bit (VIRTIO_F_VERSION_1, bit 32) set, then drives a
 * valid block read to confirm the reinit took effect.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_reinit_reduced_features(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Read offered features, just to exercise the path */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    (void)cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t high = cfg->device_feature;
    if (!(high & (1u << (VIRTIO_F_VERSION_1 - 32))))
        return TEST_SKIP;

    /* Accept only VIRTIO_F_VERSION_1 */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 1u << (VIRTIO_F_VERSION_1 - 32);
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    /* Reprogram queue 0 with a fresh ring */
    struct vring nv;
    vring_alloc(&nv, 16);
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = 16;
    virtio_store64(&cfg->queue_desc, nv.desc_phys);
    virtio_store64(&cfg->queue_avail, nv.avail_phys);
    virtio_store64(&cfg->queue_used, nv.used_phys);
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();
    nv.queue = 0;

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

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

REGISTER_TEST(S0034, VIRTIO_PCI_DEVICE_BLK, test_reinit_reduced_features,
              "Reinit accepts strictly reduced feature subset",
              VIRTIO_SPEC_V1_2, "3.1.1");
