/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0038: nonexistent_lun
 *
 * A command to a logical unit that is not present on an existing target
 * is answered either with a BAD_TARGET response or a CHECK CONDITION
 * status, never silently accepted.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_bad_lun(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 5);   /* LUN 5 does not exist */
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, NULL, 0,
                                  SCSI_DATA_NONE, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response == VIRTIO_SCSI_S_BAD_TARGET)
        return TEST_PASS;
    if (resp->response == VIRTIO_SCSI_S_OK && resp->status == 0x02)
        return TEST_PASS;
    TFAIL("response 0x%02x status 0x%02x", resp->response, resp->status);
}

REGISTER_TEST_Q(SCSI0038, VIRTIO_PCI_DEVICE_SCSI, test_scsi_bad_lun,
                "Command to a missing logical unit is refused",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);
