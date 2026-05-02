/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0058: indirect_cross_page_unmapped
 *
 * Create an indirect descriptor table that spans a page boundary where
 * the second page is unmapped. The device must either reject the
 * descriptor or fault gracefully rather than crashing when it walks
 * past the page boundary into invalid memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
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

static test_result_t test_indirect_cross_page(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    (void)status;

    /*
     * Allocate 2 pages, then unmap the second one to create an
     * accessible-then-hole layout. Place the indirect table at the
     * end of page 1 so it spans into unmapped page 2.
     */
    void *two_pages = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
    if (two_pages == MAP_FAILED)
        return TEST_SKIP;

    /* Unmap the second page */
    if (munmap((char *)two_pages + PAGE_SIZE, PAGE_SIZE) < 0)
        return TEST_SKIP;

    /*
     * Place 3 indirect descriptors starting 32 bytes before page end.
     * Each desc is 16 bytes, so desc[0] fits in page 1, desc[1] spans
     * into unmapped page 2.
     */
    struct vring_desc *indirect = (struct vring_desc *)
        ((char *)two_pages + PAGE_SIZE - 2 * sizeof(struct vring_desc));

    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = data_phys;
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;
    /* desc[2] would be in unmapped memory - we claim 3 descs */

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Indirect descriptor claiming 3 entries but only 2 are accessible */
    vring_raw_set_desc(vr, 0, indirect_phys, 3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0058, VIRTIO_PCI_DEVICE_BLK, test_indirect_cross_page,
              "Indirect table spanning page boundary with 2nd page unmapped",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
