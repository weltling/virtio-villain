/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0097: tmf_no_writable_resp
 *
 * Submit a task management request whose whole chain is read only, so
 * the device has no writable buffer for the response. The device must
 * refuse it without a completion; the host must stay alive.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_tmf_no_resp(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET;
    scsi_set_lun(req->lun, 0, 0);

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), 0, 0);
    vring_submit(vr, 0);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a request with no writable response");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0097, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tmf_no_resp,
                "Task management with no writable response is refused safely",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);
