/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0082: write12_past_capacity
 *
 * A WRITE(12) beyond the last logical block is answered with a CHECK
 * CONDITION status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_write12_past(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(data, 0, 512);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xAA;      /* WRITE(12) */
    req->cdb[2] = 0x01;      /* LBA far past capacity */
    req->cdb[9] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_OUT, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (resp->status != 0x02)
        TFAIL("status 0x%02x, expected CHECK CONDITION", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0082, VIRTIO_PCI_DEVICE_SCSI, test_scsi_write12_past,
                "WRITE(12) past capacity returns CHECK CONDITION",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);
