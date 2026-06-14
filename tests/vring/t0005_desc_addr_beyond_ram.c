/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0005: desc_addr_beyond_ram
 *
 * Set a descriptor's guest physical address to a value far beyond the
 * VM's RAM size. The VMM must translate GPAs to host virtual addresses
 * for DMA; if it skips bounds validation, the translated pointer is
 * invalid and any access through it causes a SIGSEGV in the VMM process.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_addr_beyond_ram(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Descriptor chain:
     *   [0] valid header (readable)
     *   [1] data buffer at GPA 0xFFFF_FFFF_FFFF_0000 (writable)
     *   [2] status byte (writable, valid)
     *
     * The data buffer GPA is far beyond any reasonable guest RAM.
     * A VMM that does GPA-to-HVA translation without checking the
     * GPA falls within a valid memory slot will segfault.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFF0000ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0005, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_beyond_ram,
              "Descriptor GPA beyond guest RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
