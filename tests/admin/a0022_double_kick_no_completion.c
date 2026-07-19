/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0022: admin_double_kick_no_completion
 *
 * Submit a single admin command, kick once, then kick a second
 * time before any completion arrives. Spec 1.3 chapter 9.4 says
 * the second kick is a no op when no new descriptors are
 * available. The command must complete exactly once.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_double_kick(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    int progressed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        uint16_t now = vr->used->idx;
        if (now != before) {
            if (progressed && now != (uint16_t)(before + 1))
                TFAIL("progressed && now != (uint16_t)(before + 1)");
            progressed = 1;
            if (now == (uint16_t)(before + 1))
                break;
        }
        elapsed += 10000;
    }

    if (!progressed) {
        if (dev->common->device_status == 0)
            TWEDGED("dev->common->device_status == 0");
        TREJECT("!progressed");
    }

    /* Wait a small extra window to confirm no double completion */
    for (int i = 0; i < 20; i++) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != (uint16_t)(before + 1))
            TFAIL("vr->used->idx != (uint16_t)(before + 1)");
    }

    return TEST_PASS;
}

REGISTER_TEST(A0022, VIRTIO_PCI_DEVICE_BLK, test_admin_double_kick,
              "Two kicks for a single admin command",
              VIRTIO_SPEC_V1_3, "2.13");
