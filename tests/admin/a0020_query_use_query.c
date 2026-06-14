/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0020: LIST_QUERY then LIST_USE then second LIST_QUERY
 *
 * Spec 9.4 says LIST_QUERY enumerates available admin command
 * groups and LIST_USE binds the driver to one. A second
 * LIST_QUERY after a LIST_USE round must still succeed without
 * confusing the device, so the driver can re enumerate at any
 * time. Submit the three requests as separate chains in one
 * batch and verify all three complete.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define ADMIN_OPCODE_LIST_QUERY 0x0001
#define ADMIN_OPCODE_LIST_USE   0x0000

static void mk_chain(struct vring *vr, uint16_t head,
                     uint64_t in_phys, size_t in_len,
                     uint64_t out_phys, size_t out_len)
{
    vring_raw_set_desc(vr, head, in_phys, in_len,
                       VRING_DESC_F_NEXT, head + 1);
    vring_raw_set_desc(vr, head + 1, out_phys, out_len,
                       VRING_DESC_F_WRITE, 0);
}

static test_result_t test_admin_query_use_query(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct admin_hdr *q1 = vv_alloc_pages(1);
    struct admin_hdr *u  = vv_alloc_pages(1);
    struct admin_hdr *q2 = vv_alloc_pages(1);
    struct admin_resp *r1 = vv_alloc_pages(1);
    struct admin_resp *ru = vv_alloc_pages(1);
    struct admin_resp *r2 = vv_alloc_pages(1);

    memset(q1, 0, sizeof(*q1));
    memset(u, 0, sizeof(*u));
    memset(q2, 0, sizeof(*q2));
    r1->status = 0xFFFF;
    ru->status = 0xFFFF;
    r2->status = 0xFFFF;

    q1->opcode = ADMIN_OPCODE_LIST_QUERY;
    u->opcode  = ADMIN_OPCODE_LIST_USE;
    q2->opcode = ADMIN_OPCODE_LIST_QUERY;

    mk_chain(vr, 0, vv_virt_to_phys(q1), sizeof(*q1),
             vv_virt_to_phys(r1), sizeof(*r1));
    mk_chain(vr, 2, vv_virt_to_phys(u), sizeof(*u),
             vv_virt_to_phys(ru), sizeof(*ru));
    mk_chain(vr, 4, vv_virt_to_phys(q2), sizeof(*q2),
             vv_virt_to_phys(r2), sizeof(*r2));

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0020, VIRTIO_PCI_DEVICE_BLK, test_admin_query_use_query,
              "LIST_QUERY then LIST_USE then LIST_QUERY in one batch",
              VIRTIO_SPEC_V1_2, "9.4");
