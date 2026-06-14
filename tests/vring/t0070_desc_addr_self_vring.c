/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0070: desc_addr_self_vring
 *
 * Set a descriptor address pointing into the virtqueue's own memory
 * region (desc table, avail ring, or used ring). If the device
 * writes DMA data into its own queue structures, it may corrupt
 * subsequent descriptor processing or create infinite loops.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_addr_self_vring(struct virtio_dev *dev,
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
     * Point the data descriptor at the descriptor table itself.
     * A 512-byte DMA write here would overwrite desc entries.
     */
    uint64_t desc_table_phys = vv_virt_to_phys(vr->desc);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, desc_table_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0070, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_self_vring,
              "Descriptor address pointing into own virtqueue memory",
              VIRTIO_SPEC_V1_2, "2.7.5");
