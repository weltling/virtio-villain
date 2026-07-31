/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0127: invalid_lun_format
 *
 * A request whose LUN field does not use the required addressing method
 * byte of 1 is refused, either without a completion or with a
 * BAD_TARGET response or CHECK CONDITION status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_bad_lun_format(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    /* Addressing method byte must be 1; use 0 to make the LUN invalid. */
    req->lun[0] = 0;
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, NULL, 0,
                                  SCSI_DATA_NONE, 0);
    if (r != TEST_PASS)
        return TEST_PASS;    /* refused without a completion is fine */
    __sync_synchronize();
    if (resp->response == VIRTIO_SCSI_S_BAD_TARGET)
        return TEST_PASS;
    if (resp->response == VIRTIO_SCSI_S_OK && resp->status == 0x02)
        return TEST_PASS;
    TFAIL("response 0x%02x status 0x%02x", resp->response, resp->status);
}

REGISTER_TEST_Q(SCSI0127, VIRTIO_PCI_DEVICE_SCSI, test_scsi_bad_lun_format,
                "Invalid LUN addressing method is refused",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);
