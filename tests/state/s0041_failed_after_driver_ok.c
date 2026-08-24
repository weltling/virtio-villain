/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0041: FAILED set after DRIVER_OK
 *
 * Spec 2.1.2 says the driver may indicate FAILED at any point,
 * including after a successful init. Set FAILED while the device
 * is fully operational and verify the bit is observable. Then
 * reset and confirm a fresh reinit completes, exercising the
 * post DRIVER_OK fail path which is rare in normal driver flow.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_failed_after_driver_ok(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Confirm we are post DRIVER_OK */
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    /* Set FAILED on top of DRIVER_OK */
    cfg->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FAILED))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FAILED)");

    /* Reset */
    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Reinit and run a valid I/O */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    struct vring nv;
    vring_alloc(&nv, 16);
    cfg->queue_select = 0;
    cfg->queue_size = 16;
    virtio_store64(&cfg->queue_desc, nv.desc_phys);
    virtio_store64(&cfg->queue_avail, nv.avail_phys);
    virtio_store64(&cfg->queue_used, nv.used_phys);
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    nv.queue = 0;
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    (void)vr;

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

REGISTER_TEST(S0041, VIRTIO_PCI_DEVICE_BLK, test_failed_after_driver_ok,
              "FAILED set after DRIVER_OK then reset and reinit",
              VIRTIO_SPEC_V1_2, "2.1.2");
