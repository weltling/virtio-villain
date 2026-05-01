/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0001: desc_chain_loop
 *
 * Create a descriptor chain that loops back to itself. The descriptor's
 * next field points to its own index, forming a cycle. A VMM that walks
 * the chain without tracking visited indices will loop forever (hang or
 * 100% CPU in the virtio worker thread).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

/* virtio-blk request header */
struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_desc_chain_loop(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 512);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    /*
     * Descriptor chain: [0] header -> [1] data -> [1] (loop!)
     * Descriptor 1's next points back to itself.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);

    /* Submit head 0 */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0001, VIRTIO_PCI_DEVICE_BLK, test_desc_chain_loop,
              "Descriptor chain loop back to self",
              VIRTIO_SPEC_V1_2, "2.7.5");
