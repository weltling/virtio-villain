/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0129: wrapping descriptor entry inside an indirect table.
 *
 * T0008, T0105 and T0128 wrap a descriptor in the direct ring. Here the
 * wrapping descriptor lives inside an indirect table, so the device
 * reads the table first and then processes an entry whose addr plus len
 * wraps past 2^64. The range is built through the indirect read path
 * rather than the direct chain walk. The data entry base sits near the
 * top of the address space and the length makes addr plus len wrap to a
 * low value. The device must reject the range or stay alive.
 *
 * Spec 2.7.5.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

struct idesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

static test_result_t test_indirect_entry_wrap(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_F_INDIRECT_DESC)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    /*
     * Indirect table: header valid, data entry wraps past 2^64
     * (base 2^64 - 4096, len 0x2000 -> end 0x1000), status valid.
     */
    struct idesc *itbl = vv_alloc_pages(1);
    itbl[0].addr = vv_virt_to_phys(hdr);
    itbl[0].len = sizeof(*hdr);
    itbl[0].flags = VRING_DESC_F_NEXT;
    itbl[0].next = 1;
    itbl[1].addr = 0xFFFFFFFFFFFFF000ULL;
    itbl[1].len = 0x2000;
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

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(T0129, VIRTIO_PCI_DEVICE_BLK, test_indirect_entry_wrap,
                       "Wrapping descriptor entry inside an indirect table",
                       VIRTIO_SPEC_V1_2, "2.7.5.3",
                       (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
