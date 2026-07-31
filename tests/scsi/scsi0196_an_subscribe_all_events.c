/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0196: an_subscribe_all_events
 *
 * An asynchronous notification subscribe requesting every event bit
 * completes with a defined response code.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_an_sub_all(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_ctrl_an_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_AN_SUBSCRIBE;
    scsi_set_lun(req->lun, 0, 0);
    req->event_requested = 0xFFFFFFFF;
    resp->response = 0xAA;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (!scsi_tmf_response_valid(resp->response))
        TFAIL("response 0x%02x", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0196, VIRTIO_PCI_DEVICE_SCSI, test_scsi_an_sub_all,
                "Notification subscribe for all events completes",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);
