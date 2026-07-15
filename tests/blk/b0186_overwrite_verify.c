/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0186: overwrite sector then read verifies new data.
 *
 * Write pattern A to sector 0, then overwrite with pattern B, then
 * read back and verify pattern B is returned. Tests that the device
 * does not cache stale data across writes to the same sector.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_overwrite(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* Write pattern A */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0x11, 512); *st = 0xFF;

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
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write1 status %u", *st);

    /* Overwrite with pattern B */
    memset(data, 0x99, 512); *st = 0xFF;
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write2 status %u", *st);

    /* Read back */
    hdr->type = VIRTIO_BLK_T_IN;
    memset(data, 0, 512); *st = 0xFF;

    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 512; i++) {
        if (data[i] != 0x99)
            TFAIL("byte %d: 0x%02x != 0x99", i, data[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0186, VIRTIO_PCI_DEVICE_BLK, test_blk_overwrite,
              "Overwrite sector then read verifies new data",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
