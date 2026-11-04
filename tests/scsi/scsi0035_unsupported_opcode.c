/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0035: unsupported_opcode
 *
 * An unsupported operation code is answered with a CHECK CONDITION
 * status rather than being silently accepted.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_bad_opcode(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xFF;      /* not a defined operation code */

    VV_TRY(scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, seq));
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (resp->status != 0x02)
        TFAIL("status 0x%02x, expected CHECK CONDITION", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0035, VIRTIO_PCI_DEVICE_SCSI, test_scsi_bad_opcode,
                "Unsupported opcode returns CHECK CONDITION",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);
