/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0230: FUA write with REQ_FLAGS then read returns the data.
 *
 * Spec 5.2.6: when VIRTIO_BLK_F_REQ_FLAGS and
 * VIRTIO_BLK_F_REQ_FLAGS_OUT_FUA are negotiated, a VIRTIO_BLK_T_OUT
 * request may set the VIRTIO_BLK_REQ_FLAG_OUT_FUA bit in the flags
 * word (the u32 that was formerly ioprio). Write a sector with the
 * FUA flag set, then read it back and verify the data persisted.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_fua_write_read(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_REQ_FLAGS) ||
        !virtio_pci_feature_offered(dev, VIRTIO_BLK_F_REQ_FLAGS_OUT_FUA))
        return TEST_SKIP;
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 1) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* Write sector 0 with the Force Unit Access flag set. */
    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 1u << VIRTIO_BLK_REQ_FLAG_OUT_FUA; /* flags word */
    hdr->sector = 0;
    memset(data, 0x6C, 512);
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("FUA write status %u", *st);

    /* Read the sector back and verify the pattern. */
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 512);
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);
    for (int i = 0; i < 512; i++)
        if (data[i] != 0x6C)
            TFAIL("byte %d: 0x%02x != 0x6c", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0230, VIRTIO_PCI_DEVICE_BLK, test_blk_fua_write_read,
              "FUA write with REQ_FLAGS then read returns the data",
              VIRTIO_SPEC_V1_4, "5.2.6",
              (1ULL << VIRTIO_BLK_F_REQ_FLAGS) |
              (1ULL << VIRTIO_BLK_F_REQ_FLAGS_OUT_FUA), 0);
