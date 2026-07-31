/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0168: mode_select6
 *
 * MODE SELECT(6) with a minimal parameter header returns a defined
 * completion status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_mode_select6(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(data, 0, 4);      /* four byte mode parameter header */

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x15;      /* MODE SELECT(6) */
    req->cdb[1] = 0x10;      /* page format */
    req->cdb[4] = 4;         /* parameter list length */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 4,
                                  SCSI_DATA_OUT, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0168, VIRTIO_PCI_DEVICE_SCSI, test_scsi_mode_select6,
                "MODE SELECT(6) returns a defined status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);
