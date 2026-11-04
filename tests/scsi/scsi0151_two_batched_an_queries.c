/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0151: two_batched_an_queries
 *
 * Post two asynchronous notification queries on the control queue
 * before kicking and confirm the device answers both with defined
 * response codes.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_two_an(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_scsi_ctrl_an_req *req0 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_req *req1 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_resp *resp0 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_resp *resp1 = vv_alloc_pages(1);

    req0->type = VIRTIO_SCSI_T_AN_QUERY;
    scsi_set_lun(req0->lun, 0, 0);
    memcpy(req1, req0, sizeof(*req0));
    resp0->response = 0xAA;
    resp1->response = 0xAA;

    vv_desc(vr, 0, req0, sizeof(*req0), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp0, sizeof(*resp0), VRING_DESC_F_WRITE, 0);
    vv_desc(vr, 2, req1, sizeof(*req1), VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, resp1, sizeof(*resp1), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    VV_TRY(vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS));
    __sync_synchronize();
    if (!scsi_tmf_response_valid(resp0->response) ||
        !scsi_tmf_response_valid(resp1->response))
        TFAIL("responses 0x%02x 0x%02x", resp0->response, resp1->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0151, VIRTIO_PCI_DEVICE_SCSI, test_scsi_two_an,
                "Two batched notification queries both complete",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);
