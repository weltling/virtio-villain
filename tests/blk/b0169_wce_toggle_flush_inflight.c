/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0169: wce_toggle_flush_inflight
 *
 * Submit a WRITE followed by a FLUSH, then toggle the WCE field from
 * writeback to writethrough while both requests are still inflight.
 * Spec 5.2.6.2 says changing writeback between submission and
 * completion makes the write's persistence "undefined" but the
 * device must not crash, hang or corrupt unrelated state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wce_toggle_flush_inflight(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_CONFIG_WCE)))
        return TEST_SKIP;
    if (!(offered & (1U << VIRTIO_BLK_F_FLUSH)))
        return TEST_SKIP;
    if (dev->device_cfg_length <= VIRTIO_BLK_CFG_WCE_OFFSET)
        return TEST_SKIP;

    volatile uint8_t *wce = (volatile uint8_t *)dev->device_cfg +
                            VIRTIO_BLK_CFG_WCE_OFFSET;

    /* Ensure writeback mode (1) before we start */
    *wce = 1;
    __sync_synchronize();

    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wstatus = vv_alloc_pages(1);

    struct virtio_blk_outhdr *fhdr = vv_alloc_pages(1);
    uint8_t *fstatus = vv_alloc_pages(1);

    /* WRITE request: sector 0, 512 bytes */
    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    memset(wdata, 0x42, 512);
    *wstatus = 0xFF;

    /* FLUSH request */
    fhdr->type = VIRTIO_BLK_T_FLUSH;
    fhdr->ioprio = 0;
    fhdr->sector = 0;
    *fstatus = 0xFF;

    /* Chain 0: WRITE (descs 0,1,2) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(whdr), sizeof(*whdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(wdata), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(wstatus), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 1: FLUSH (descs 3,4) */
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(fhdr), sizeof(*fhdr),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(fstatus), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    /* Record baseline, kick, then toggle WCE while inflight */
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Toggle WCE to writethrough while I/O in flight */
    *wce = 0;
    __sync_synchronize();

    /* Wait for both completions */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if ((uint16_t)(vr->used->idx - before) >= 2)
            return TEST_PASS;
        elapsed += 10000;
    }
    return TEST_WEDGED;
}

REGISTER_TEST_REQUIRES(B0169, VIRTIO_PCI_DEVICE_BLK,
              test_blk_wce_toggle_flush_inflight,
              "Toggle WCE from writeback to writethrough while flush inflight",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_CONFIG_WCE) | (1ULL << VIRTIO_BLK_F_FLUSH),
              0);
