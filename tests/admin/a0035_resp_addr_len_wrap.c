/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0035: admin command response writable addr plus len wraps 2^64.
 *
 * Sibling to A0013, which gives the response a tiny buffer. Here the
 * response descriptor base sits near the top of the address space and
 * the length makes addr plus len wrap to a low value, so a device that
 * computes the end with a plain addition gets a small wrapped result and
 * a naive bounds check passes. The device must not access memory outside
 * the guest mapping or crash the VMM. Completing, silently rejecting, or
 * wedging the queue are all acceptable.
 *
 * Spec 2.13.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_admin_resp_addr_len_wrap(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0035, VIRTIO_PCI_DEVICE_BLK, test_admin_resp_addr_len_wrap,
              "Admin response writable addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_3, "2.13");