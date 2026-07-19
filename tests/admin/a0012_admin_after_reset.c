/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0012: admin_cmd_after_device_reset
 *
 * Reset the device, reinitialize, then submit an admin command.
 * Tests whether the admin virtqueue is usable after a reset cycle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_after_reset(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset the device */
    virtio_pci_reset(dev);

    /* Reinitialize: ACKNOWLEDGE + DRIVER + FEATURES_OK + DRIVER_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Accept no features */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Set up a fresh queue for admin */
    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    /* Submit an admin command */
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    memset(result, 0xFF, 64);

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0012, VIRTIO_PCI_DEVICE_BLK, test_admin_after_reset,
              "Admin command submitted after device reset cycle",
              VIRTIO_SPEC_V1_3, "2.13");
