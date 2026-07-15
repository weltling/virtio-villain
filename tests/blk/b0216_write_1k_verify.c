/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0216: write 1024 bytes (two sectors) then read verify.
 *
 * Write 1024 bytes starting at sector 0 in a single request, read
 * both sectors back, verify all 1024 bytes match.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_1k_rw(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    for (int i = 0; i < 1024; i++) data[i] = (uint8_t)(i & 0xFF);
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 1024,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    hdr->type = VIRTIO_BLK_T_IN;
    memset(data, 0, 1024); *st = 0xFF;
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 1024,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 1024; i++)
        if (data[i] != (uint8_t)(i & 0xFF))
            TFAIL("byte %d: 0x%02x != 0x%02x", i, data[i], (uint8_t)(i & 0xFF));

    return TEST_PASS;
}

REGISTER_TEST(B0216, VIRTIO_PCI_DEVICE_BLK, test_blk_1k_rw,
              "Write 1024 bytes then read verify",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
