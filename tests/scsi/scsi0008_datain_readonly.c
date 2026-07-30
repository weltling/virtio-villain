/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0008: datain_readonly
 *
 * Issue a READ whose data-in buffer descriptor is not marked writable,
 * so the descriptor direction contradicts the command. The device must
 * not write past a read only descriptor; the host must stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_datain_readonly(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 0x01;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Data-in buffer left read only despite the READ direction. */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 512, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* The device must not write into a read only descriptor. It refuses
     * the request without a completion; the host stays alive. */
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a READ into a read only buffer");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0008, VIRTIO_PCI_DEVICE_SCSI, test_scsi_datain_readonly,
                "READ with a read only data-in buffer is handled safely",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);
