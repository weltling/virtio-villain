/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0034: Indirect packed table that starts on the last 16 bytes of a
 * page with entries crossing into the next (unmapped) page.
 *
 * Spec 2.8.7: The device must handle indirect tables that extend
 * beyond accessible memory without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_indirect_page_boundary(struct virtio_dev *dev,
                                                        struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /*
     * Allocate 2 pages, unmap the second. Place indirect table at the
     * end of page 1 so claimed entries span into unmapped page 2.
     */
    void *two_pages = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
    if (two_pages == MAP_FAILED)
        return TEST_SKIP;

    if (munmap((char *)two_pages + PAGE_SIZE, PAGE_SIZE) < 0)
        return TEST_SKIP;

    /*
     * Each packed desc is 16 bytes. Place table at offset PAGE_SIZE - 16
     * so only the first entry fits in mapped memory. We claim 3 entries.
     */
    struct vring_packed_desc *indirect = (struct vring_packed_desc *)
        ((char *)two_pages + PAGE_SIZE - sizeof(struct vring_packed_desc));

    /* Only first entry is accessible */
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].id = 0;
    indirect[0].flags = VRING_PACKED_DESC_F_NEXT;
    /* entries [1] and [2] are in unmapped memory */

    uint64_t indirect_phys = vv_virt_to_phys(indirect);
    uint8_t wrap = vr->wrap_counter;
    uint16_t check_idx = vr->next_avail;

    /* Submit indirect claiming 3 entries, only 1 is accessible */
    vring_packed_set_desc(vr, vr->next_avail, indirect_phys,
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, check_idx, wrap,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0034, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_page_boundary,
                     "Packed indirect table crossing into unmapped page",
                     VIRTIO_SPEC_V1_2, "2.8.7");
