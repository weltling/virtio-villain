/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0223: write and read 8192 bytes across two page descriptors.
 *
 * Write 8K of data using one descriptor per physical page (pages
 * from mmap are not guaranteed physically contiguous), read it
 * back the same way, and verify all 8192 bytes. Tests multi
 * descriptor data assembly with content verification.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_8k(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 16) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *p0 = vv_alloc_pages(1);
    uint8_t *p1 = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    for (int i = 0; i < 4096; i++) p0[i] = (uint8_t)((i * 3 + 7) & 0xFF);
    for (int i = 0; i < 4096; i++)
        p1[i] = (uint8_t)(((i + 4096) * 3 + 7) & 0xFF);

    /* WRITE: hdr -> p0 -> p1 -> status */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p0), 4096,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(p1), 4096,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* READ back into the same pages */
    memset(p0, 0, 4096); memset(p1, 0, 4096); *st = 0xFF;
    hdr->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p0), 4096,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(p1), 4096,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 4096; i++) {
        uint8_t e0 = (uint8_t)((i * 3 + 7) & 0xFF);
        if (p0[i] != e0) TFAIL("p0 byte %d: 0x%02x != 0x%02x", i, p0[i], e0);
        uint8_t e1 = (uint8_t)(((i + 4096) * 3 + 7) & 0xFF);
        if (p1[i] != e1) TFAIL("p1 byte %d: 0x%02x != 0x%02x", i, p1[i], e1);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0223, VIRTIO_PCI_DEVICE_BLK, test_blk_8k,
              "Write and read 8192 bytes across two page descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
