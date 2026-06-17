/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0213: blk_status_write_indirect
 *
 * Submit a three descriptor IN request whose status descriptor carries
 * VRING_DESC_F_INDIRECT together with VRING_DESC_F_WRITE. The header and
 * data descriptors are well formed, so the device starts parsing the
 * chain, then reaches a status slot that claims to be an indirect table
 * of one byte. That length is not a valid indirect table, so the parse
 * fails partway through. The device must decline the request and return
 * the head to the used ring without crashing or wedging the queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_status_write_indirect(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    void *data = vv_alloc_pages(1);
    void *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Readable header, then writable data, then a status slot that sets
     * VRING_DESC_F_INDIRECT on top of VRING_DESC_F_WRITE. */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 1024,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0213, VIRTIO_PCI_DEVICE_BLK, test_blk_status_write_indirect,
              "Status descriptor with WRITE and INDIRECT flags combined",
              VIRTIO_SPEC_V1_2, "5.2.6");
