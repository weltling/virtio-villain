/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0057: WRITE with VIRTIO_BLK_F_RO feature (spec 5.2.5)
 *
 * If the device exposes VIRTIO_BLK_F_RO (read-only), attempt a
 * write. The device should report an error status or reject.
 * Also verify reads still work and the rejected write leaves data unchanged.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_readonly(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    if (!(offered & (1U << VIRTIO_BLK_F_RO)))
        return TEST_SKIP; /* device is not read-only */

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint8_t original[512];

    hdr->sector = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    for (unsigned phase = 0; phase < 3; phase++) {
        int writing = phase == 1;
        hdr->type = writing ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
        if (writing) {
            for (unsigned i = 0; i < sizeof(original); i++)
                data[i] = original[i] ^ 0xFF;
        } else {
            memset(data, 0xFF, sizeof(original));
        }
        *st = 0xFF;
        vv_desc(vr, 1, data, sizeof(original),
                VRING_DESC_F_NEXT | (writing ? 0 : VRING_DESC_F_WRITE), 2);
        uint16_t idx = vr->avail->idx;
        vring_raw_set_avail(vr, idx % vr->size, 0);
        vring_raw_set_avail_idx(vr, idx + 1);

        VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
        if (writing) {
            if (*st != VIRTIO_BLK_S_IOERR && *st != VIRTIO_BLK_S_UNSUPP)
                TFAIL("read-only write returned status %u", *st);
        } else {
            if (*st != VIRTIO_BLK_S_OK)
                TFAIL("read-only read phase %u returned status %u", phase, *st);
            if (phase == 0)
                memcpy(original, data, sizeof(original));
            else if (memcmp(original, data, sizeof(original)) != 0)
                TFAIL("rejected write changed read-only disk data");
        }
    }
    puts("vv-ro-checked: write rejected, reads succeed, data unchanged");
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0057, VIRTIO_PCI_DEVICE_BLK, test_blk_write_readonly,
              "WRITE to read-only device (VIRTIO_BLK_F_RO set)",
              VIRTIO_SPEC_V1_2, "5.2.5",
              (1ULL << VIRTIO_BLK_F_RO), 0);
