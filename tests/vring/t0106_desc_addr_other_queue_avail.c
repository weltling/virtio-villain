/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0106: desc_addr_other_queue_avail
 *
 * Point a writable data descriptor on queue 0 at the available ring
 * of a second queue. A blk read DMAs disk bytes into queue 1's avail
 * ring. If the device honors the address without checking that it
 * lands on another queue's control structure, the DMA corrupts the
 * neighbouring ring. Sibling of T0070 (own queue) for the cross queue
 * case.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_addr_other_queue_avail(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP; /* need a second queue to target */

    /* Bring up queue 1 so its avail ring is live memory. */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Data descriptor writes 512 bytes into queue 1's avail ring. */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vr2.avail_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0106, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_other_queue_avail,
              "Descriptor address pointing into another queue's avail ring",
              VIRTIO_SPEC_V1_2, "2.7.5");
