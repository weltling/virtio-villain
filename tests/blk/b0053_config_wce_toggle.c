/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0053: config_wce_toggle
 *
 * Toggle the writeback/writethrough cache mode via the WCE
 * (Write Cache Enable) config field if VIRTIO_BLK_F_CONFIG_WCE
 * is offered. Write 0 then 1 and verify the device accepts both.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_blk_config_wce_toggle(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if device offers CONFIG_WCE */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_CONFIG_WCE)))
        return TEST_SKIP;

    if (dev->device_cfg_length <= VIRTIO_BLK_CFG_WCE_OFFSET)
        return TEST_SKIP;

    volatile uint8_t *wce = (volatile uint8_t *)dev->device_cfg +
                            VIRTIO_BLK_CFG_WCE_OFFSET;

    /* Read current value */
    uint8_t orig = *wce;

    /* Toggle to writethrough (0) */
    *wce = 0;
    __sync_synchronize();
    usleep(10000);

    /* Toggle to writeback (1) */
    *wce = 1;
    __sync_synchronize();
    usleep(10000);

    /* Restore */
    *wce = orig;
    __sync_synchronize();

    /* Verify device is still functional */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(B0053, VIRTIO_PCI_DEVICE_BLK, test_blk_config_wce_toggle,
              "Toggle CONFIG_WCE writeback/writethrough mode",
              VIRTIO_SPEC_V1_2, "5.2.4",
              (1ULL << VIRTIO_BLK_F_CONFIG_WCE), 0);
