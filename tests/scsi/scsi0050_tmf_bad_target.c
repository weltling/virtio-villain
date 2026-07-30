/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0050: tmf_bad_target
 *
 * A task management function addressed to a target with no logical
 * unit is answered with a BAD_TARGET or INCORRECT_LUN response code,
 * never a function complete.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_tmf_bad_target(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET;
    scsi_set_lun(req->lun, 5, 0);   /* target 5 has no logical unit */

    test_result_t r = scsi_do_tmf(dev, vr, req, resp, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_BAD_TARGET &&
        resp->response != VIRTIO_SCSI_S_INCORRECT_LUN)
        TFAIL("response 0x%02x, expected bad target", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0050, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tmf_bad_target,
                "Task management to an absent target is refused",
                VIRTIO_SPEC_V1_2, "5.6.6.2", 0);
