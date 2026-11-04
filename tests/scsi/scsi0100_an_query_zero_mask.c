/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0100: an_query_zero_mask
 *
 * An asynchronous notification query with an empty event mask completes
 * with a defined response code.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_an_query_zero(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_ctrl_an_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_SCSI_T_AN_QUERY;
    scsi_set_lun(req->lun, 0, 0);
    req->event_requested = 0;
    resp->response = 0xAA;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));
    __sync_synchronize();
    if (!scsi_tmf_response_valid(resp->response))
        TFAIL("response 0x%02x", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0100, VIRTIO_PCI_DEVICE_SCSI, test_scsi_an_query_zero,
                "Asynchronous notification query with an empty mask completes",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);
