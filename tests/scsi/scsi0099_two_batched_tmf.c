/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0099: two_batched_tmf
 *
 * Post two task management requests before kicking and confirm the
 * device answers both with defined response codes.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_two_tmf(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req0 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_req *req1 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp0 = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp1 = vv_alloc_pages(1);

    memset(req0, 0, sizeof(*req0));
    req0->type = VIRTIO_SCSI_T_TMF;
    req0->subtype = VIRTIO_SCSI_T_TMF_ABORT_TASK_SET;
    scsi_set_lun(req0->lun, 0, 0);
    memcpy(req1, req0, sizeof(*req0));
    resp0->response = 0xAA;
    resp1->response = 0xAA;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req0), sizeof(*req0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp0), sizeof(*resp0),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req1), sizeof(*req1),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp1), sizeof(*resp1),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (!scsi_tmf_response_valid(resp0->response) ||
        !scsi_tmf_response_valid(resp1->response))
        TFAIL("responses 0x%02x 0x%02x", resp0->response, resp1->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0099, VIRTIO_PCI_DEVICE_SCSI, test_scsi_two_tmf,
                "Two batched task management requests both complete",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);
