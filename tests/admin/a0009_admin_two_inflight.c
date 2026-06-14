/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0009: admin_cmd_two_inflight
 *
 * Submit two admin commands simultaneously in the avail ring (one kick).
 * Tests device handling of concurrent admin operations - some devices
 * may only support one admin command at a time.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_two_inflight(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd1 = vv_alloc_pages(1);
    uint8_t *result1 = vv_alloc_pages(1);
    struct virtio_admin_cmd_hdr *cmd2 = vv_alloc_pages(1);
    uint8_t *result2 = vv_alloc_pages(1);

    memset(cmd1, 0, sizeof(*cmd1));
    cmd1->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd1->group_type = 0;
    cmd1->group_member_id = 0;

    memset(cmd2, 0, sizeof(*cmd2));
    cmd2->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd2->group_type = 0;
    cmd2->group_member_id = 0;

    /* Chain 1: cmd1 -> result1 (descs 0-1) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd1), sizeof(*cmd1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(result1), 64,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2: cmd2 -> result2 (descs 2-3) */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(cmd2), sizeof(*cmd2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(result2), 64,
                       VRING_DESC_F_WRITE, 0);

    /* Both in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0009, VIRTIO_PCI_DEVICE_BLK, test_admin_two_inflight,
              "Two admin commands in-flight simultaneously",
              VIRTIO_SPEC_V1_3, "9.4");
