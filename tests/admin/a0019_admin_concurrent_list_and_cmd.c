/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0019: Submit LIST_USE on one descriptor chain and a regular admin
 * command simultaneously on a second chain.
 *
 * Spec 9.4: Tests device behavior when multiple admin operations
 * are in flight concurrently on the same admin queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_admin_cmd_hdr {
    uint16_t opcode;
    uint16_t group_type;
    uint16_t group;
    uint16_t command_specific_data_len;
} __attribute__((packed));

#define ADMIN_OPCODE_LIST_USE 0x0000
#define ADMIN_OPCODE_LIST_QUERY 0x0001

static test_result_t test_admin_concurrent(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    /* Chain 1: LIST_USE command */
    struct virtio_admin_cmd_hdr *cmd1 = vv_alloc_pages(1);
    uint8_t *result1 = vv_alloc_pages(1);
    memset(cmd1, 0, PAGE_SIZE);
    memset(result1, 0xFF, 64);

    cmd1->opcode = ADMIN_OPCODE_LIST_USE;
    cmd1->group_type = 1;
    cmd1->group = 0;
    cmd1->command_specific_data_len = 0;

    /* Chain 2: LIST_QUERY command */
    struct virtio_admin_cmd_hdr *cmd2 = vv_alloc_pages(1);
    uint8_t *result2 = vv_alloc_pages(1);
    memset(cmd2, 0, PAGE_SIZE);
    memset(result2, 0xFF, 64);

    cmd2->opcode = ADMIN_OPCODE_LIST_QUERY;
    cmd2->group_type = 1;
    cmd2->group = 0;
    cmd2->command_specific_data_len = 0;

    /* Set up chain 1: descs 0-1 */
    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd1), sizeof(*cmd1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result1), 64,
                       VRING_DESC_F_WRITE, 0);

    /* Set up chain 2: descs 2-3 */
    vring_raw_set_desc(&avr, 2, vv_virt_to_phys(cmd2), sizeof(*cmd2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&avr, 3, vv_virt_to_phys(result2), 64,
                       VRING_DESC_F_WRITE, 0);

    /* Make both available simultaneously */
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail(&avr, 1, 2);
    vring_raw_set_avail_idx(&avr, 2);

    /* Single kick for both */
    uint16_t before = avr.used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, avr.queue);

    /* Wait for at least one completion */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (avr.used->idx != before)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(A0019, VIRTIO_PCI_DEVICE_BLK, test_admin_concurrent,
              "Concurrent LIST_USE and admin command on same queue",
              VIRTIO_SPEC_V1_3, "9.4");
