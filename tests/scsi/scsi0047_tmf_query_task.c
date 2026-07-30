/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0047: tmf_query_task
 *
 * A QUERY TASK task management function completes with a defined
 * control queue response code.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_tmf_query_task(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = VIRTIO_SCSI_T_TMF_QUERY_TASK;
    scsi_set_lun(req->lun, 0, 0);

    test_result_t r = scsi_do_tmf(dev, vr, req, resp, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (!scsi_tmf_response_valid(resp->response))
        TFAIL("response 0x%02x", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0047, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tmf_query_task,
                "QUERY TASK returns a defined response code",
                VIRTIO_SPEC_V1_2, "5.6.6.2", 0);
