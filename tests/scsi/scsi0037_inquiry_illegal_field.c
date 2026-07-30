/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0037: inquiry_illegal_field
 *
 * INQUIRY with the EVPD bit clear but a nonzero page code is an illegal
 * request and is answered with a CHECK CONDITION status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_illegal(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x00;      /* EVPD clear */
    req->cdb[2] = 0x01;      /* page code must be zero when EVPD is clear */
    req->cdb[4] = 36;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 36,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (resp->status != 0x02)
        TFAIL("status 0x%02x, expected CHECK CONDITION", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0037, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_illegal,
                "INQUIRY with an illegal field returns CHECK CONDITION",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);
