/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0108: indirect_table_other_queue_desc
 *
 * Place the indirect descriptor table inside a second queue's
 * descriptor table page, then reference it from an INDIRECT
 * descriptor on queue 0. The indirect table aliases another live
 * queue's descriptor area. The device must read the indirect table
 * without treating the aliased page as that queue's descriptors or
 * corrupting it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_indirect_table_other_queue_desc(struct virtio_dev *dev,
                                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP; /* need a second queue to alias */

    /* Bring up queue 1; its descriptor table is live memory. */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /*
     * Build the indirect table inside queue 1's descriptor table
     * rather than in a fresh page. The first three slots double as
     * queue 1 descriptors and as the indirect chain for queue 0.
     */
    struct vring_desc *indirect = vr2.desc;

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;

    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;

    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    /* Queue 0 head points at queue 1's descriptor page as indirect. */
    vring_raw_set_desc(vr, 0, vr2.desc_phys,
                       3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0108, VIRTIO_PCI_DEVICE_BLK, test_indirect_table_other_queue_desc,
              "Indirect table located in another queue's descriptor page",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
