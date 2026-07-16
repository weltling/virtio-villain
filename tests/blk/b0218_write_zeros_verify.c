/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0218: write all zeros then read verify.
 *
 * Write 512 bytes of 0x00 then read back and verify. Tests the
 * device distinguishes written zeros from uninitialized storage.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_zeros(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* First write non-zero so we know zeros are fresh */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0xCD, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Now write zeros */
    memset(data, 0x00, 512); *st = 0xFF;
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* Read back */
    hdr->type = VIRTIO_BLK_T_IN;
    memset(data, 0xEE, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 512; i++)
        if (data[i] != 0x00)
            TFAIL("byte %d: 0x%02x != 0x00", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0218, VIRTIO_PCI_DEVICE_BLK, test_blk_zeros,
              "Write all zeros then read verify",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
