/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0079: inquiry_unsupported_vpd
 *
 * INQUIRY for an unsupported vital product data page is answered with a
 * CHECK CONDITION status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_bad_vpd(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0xEE;      /* unsupported page */
    req->cdb[4] = 64;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 64,
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

REGISTER_TEST_Q(SCSI0079, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_bad_vpd,
                "INQUIRY for an unsupported VPD page returns CHECK CONDITION",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);
