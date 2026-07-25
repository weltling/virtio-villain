/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0231: blk_req_flags_ignored_without_feature
 *
 * Spec 5.2.6: the request flags word (the u32 that was ioprio in the
 * legacy layout) only carries flags such as OUT_FUA when
 * VIRTIO_BLK_F_REQ_FLAGS is negotiated. Without that feature the
 * device MUST treat the field as the legacy ioprio hint and ignore
 * it. The harness negotiates only VERSION_1 here, so REQ_FLAGS is not
 * negotiated. Write a sector with the OUT_FUA bit pattern set in that
 * field, then read the sector back and verify the write completed and
 * the data persisted, proving the device did not misread the field.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_req_flags_ignored(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 1)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* Set the OUT_FUA bit pattern in the flags/ioprio field without
     * REQ_FLAGS negotiated. The device must ignore it. */
    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 1u << VIRTIO_BLK_REQ_FLAG_OUT_FUA;
    hdr->sector = 0;
    memset(data, 0x5A, 512);
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
    if (r != TEST_PASS)
        return r;
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("write status %u with flags set but REQ_FLAGS off", *st);

    /* Read the sector back and verify the pattern persisted. */
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
    if (r != TEST_PASS)
        return r;
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("read status %u", *st);
    for (int i = 0; i < 512; i++)
        if (data[i] != 0x5A)
            TFAIL("byte %d: 0x%02x != 0x5a", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0231, VIRTIO_PCI_DEVICE_BLK, test_blk_req_flags_ignored,
              "request flags ignored when REQ_FLAGS not negotiated",
              VIRTIO_SPEC_V1_4, "5.2.6");
