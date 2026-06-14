/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0033: Admin command data buffer pointing at the device's MMIO BAR.
 *
 * Spec 9.4, 2.7.5: Descriptor addr fields name guest physical
 * addresses. Submit a LIST_USE whose data descriptor addr falls
 * inside the device's own virtio configuration BAR rather than
 * RAM. A device that DMAs through the generic memory API
 * without validating the target region can crash when the
 * backing is not regular RAM. The device must reject or handle
 * the non RAM target cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_data_in_bar(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(cmd, 0, sizeof(*cmd));
    memset(result, 0xFF, 64);

    cmd->opcode     = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1;

    /* Aim the data buffer at the device's own common config region. */
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, mmio_phys, 64,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&avr, 2, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0033, VIRTIO_PCI_DEVICE_BLK, test_admin_data_in_bar,
              "LIST_USE data descriptor points at device MMIO BAR",
              VIRTIO_SPEC_V1_3, "9.4");
