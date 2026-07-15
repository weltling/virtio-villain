/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0182: write via indirect descriptor then read back verify.
 *
 * Spec 2.7.5.3: When VIRTIO_F_INDIRECT_DESC is offered, the driver
 * may use an indirect descriptor table. Write 512 bytes to sector 0
 * using an indirect table, then read back normally and verify data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

struct indirect_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

static test_result_t test_blk_indirect_write_verify(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;

    if (!(f0 & (1U << VIRTIO_F_INDIRECT_DESC)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    for (int i = 0; i < 512; i++)
        wdata[i] = (uint8_t)(i ^ 0xF0);
    *wst = 0xFF;

    /* Build indirect table with 3 entries */
    struct indirect_desc *itbl = vv_alloc_pages(1);
    itbl[0].addr = vv_virt_to_phys(whdr);
    itbl[0].len = sizeof(*whdr);
    itbl[0].flags = VRING_DESC_F_NEXT;
    itbl[0].next = 1;

    itbl[1].addr = vv_virt_to_phys(wdata);
    itbl[1].len = 512;
    itbl[1].flags = VRING_DESC_F_NEXT;
    itbl[1].next = 2;

    itbl[2].addr = vv_virt_to_phys(wst);
    itbl[2].len = 1;
    itbl[2].flags = VRING_DESC_F_WRITE;
    itbl[2].next = 0;

    /* Single direct descriptor pointing to indirect table */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(itbl),
                       3 * sizeof(struct indirect_desc),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*wst != VIRTIO_BLK_S_OK) TFAIL("write status %u", *wst);

    /* Read back normally */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
    memset(rdata, 0, 512);
    *rst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rdata), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    for (int i = 0; i < 512; i++) {
        uint8_t e = (uint8_t)(i ^ 0xF0);
        if (rdata[i] != e)
            TFAIL("byte %d: 0x%02x != 0x%02x", i, rdata[i], e);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0182, VIRTIO_PCI_DEVICE_BLK,
              test_blk_indirect_write_verify,
              "Indirect descriptor write then read verifies data",
              VIRTIO_SPEC_V1_2, "2.7.5.3",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
