/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0028: admin_cmd_max_data_payload
 *
 * Submit an admin command with the data length field set to
 * UINT32_MAX. Spec 9.4.2 says the device must validate the
 * data_length against the actual descriptor. The device must
 * not allocate unbounded memory or crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_max_data(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Admin queue is typically the last queue */
    uint16_t aq = cfg->num_queues - 1;
    cfg->queue_select = aq;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring aq_vr;
    vring_alloc(&aq_vr, 16);
    vring_attach(dev, &aq_vr, aq);

    struct virtio_admin_cmd_full *cmd = vv_alloc_pages(1);
    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->data_length = 0xFFFFFFFFFFFFFFFFULL;

    uint8_t *status_buf = vv_alloc_pages(1);
    memset(status_buf, 0xFF, 8);

    vring_raw_set_desc(&aq_vr, 0, vv_virt_to_phys(cmd),
                       (uint32_t)sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&aq_vr, 1, vv_virt_to_phys(status_buf), 8,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&aq_vr, 0, 0);
    vring_raw_set_avail_idx(&aq_vr, 1);

    return vv_kick_and_wait(dev, &aq_vr, aq, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(A0028, VIRTIO_PCI_DEVICE_BLK, test_admin_max_data,
                "Admin command with UINT64_MAX data_length",
                VIRTIO_SPEC_V1_3, "2.13", VV_QUEUE_LAST);
