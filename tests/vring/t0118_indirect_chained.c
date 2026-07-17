/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0118: single entry indirect descriptor table.
 *
 * Spec 2.7.5.3: An indirect table may contain a single entry.
 * Use a one entry indirect table for a blk read where the single
 * indirect descriptor itself chains header, data, and status via
 * nested NEXT. Verify the request completes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

struct idesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

static test_result_t test_indirect_single(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_F_INDIRECT_DESC)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;

    /* Indirect table with 3 chained entries */
    struct idesc *itbl = vv_alloc_pages(1);
    itbl[0].addr = vv_virt_to_phys(hdr);
    itbl[0].len = sizeof(*hdr);
    itbl[0].flags = VRING_DESC_F_NEXT;
    itbl[0].next = 1;
    itbl[1].addr = vv_virt_to_phys(data);
    itbl[1].len = 512;
    itbl[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    itbl[1].next = 2;
    itbl[2].addr = vv_virt_to_phys(st);
    itbl[2].len = 1;
    itbl[2].flags = VRING_DESC_F_WRITE;
    itbl[2].next = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(itbl),
                       3 * sizeof(struct idesc),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("status %u", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(T0118, VIRTIO_PCI_DEVICE_BLK, test_indirect_single,
              "Indirect table with chained entries completes",
              VIRTIO_SPEC_V1_2, "2.7.5.3",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
